/**********************************************************************
Copyright 2022 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
********************************************************************/

#include "MaterialXDocument.h"
// std
#include <algorithm>
#include <cmath>
#include <locale>
#include <map>
#include <set>
#include <sstream>

namespace anari::rpr {

namespace {

// Minimal XML scanner //////////////////////////////////////////////////////////
//
// The documents are not parsed into a tree: the elements of a MaterialX document are found by their
// tags and only the parts which have to change are replaced, the rest of the text is kept as it is.

struct XmlElement
{
  size_t           begin{0}; // position of the '<' of the start tag
  size_t           tagEnd{0}; // position after the '>' of the start tag
  size_t           end{0}; // position after the element
  std::string_view tag;
};

// position of an attribute in the text
struct XmlAttribute
{
  bool   found{false};
  size_t begin{0}; // position of the name
  size_t end{0}; // position after the closing quote
  size_t valueBegin{0};
  size_t valueEnd{0};
};

// replacement of a part of the text
struct Edit
{
  size_t      begin;
  size_t      end;
  std::string text;
};

constexpr size_t NPOS = std::string_view::npos;

// suffix of the names of the nodes which are added to a document
const std::string GENERATED = "__rpr_";

bool isSpace(char c)
{
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool isNameEnd(char c)
{
  return isSpace(c) || c == '/' || c == '>';
}

// The numbers of a document have a decimal point, whatever the locale of the application is.
float parseFloat(const std::string &text, float defaultValue)
{
  std::istringstream stream(text);
  stream.imbue(std::locale::classic());
  float value = defaultValue;
  stream >> value;
  return stream.fail() ? defaultValue : value;
}

std::string formatFloat(float value)
{
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << value;
  return stream.str();
}

// position after the '>' of the tag which starts at 'pos', a quoted attribute value can contain '>'
size_t findTagEnd(std::string_view xml, size_t pos)
{
  char quote = 0;
  for (size_t i = pos; i < xml.size(); i++)
  {
    const char c = xml[i];
    if (quote)
    {
      if (c == quote)
        quote = 0;
    } else if (c == '"' || c == '\'')
    {
      quote = c;
    } else if (c == '>')
    {
      return i + 1;
    }
  }
  return NPOS;
}

// position of the next '<' at or after 'pos' which is not the start of a comment
size_t findTag(std::string_view xml, size_t pos)
{
  while (true)
  {
    pos = xml.find('<', pos);
    if (pos == NPOS || pos + 1 >= xml.size())
      return NPOS;
    if (xml.compare(pos, 4, "<!--") != 0)
      return pos;
    pos = xml.find("-->", pos + 4);
    if (pos == NPOS)
      return NPOS;
    pos += 3;
  }
}

// Finds the next element which starts at or after 'pos'. Returns false at the end of the text and if
// the text is not well-formed.
bool nextElement(std::string_view xml, size_t pos, XmlElement &element)
{
  while (true)
  {
    pos = findTag(xml, pos);
    if (pos == NPOS)
      return false;
    const char next = xml[pos + 1];
    if (next != '/' && next != '?' && next != '!')
      break;
    // end tag, declaration or processing instruction
    pos = findTagEnd(xml, pos);
    if (pos == NPOS)
      return false;
  }

  size_t nameEnd = pos + 1;
  while (nameEnd < xml.size() && !isNameEnd(xml[nameEnd]))
    nameEnd++;
  element.begin  = pos;
  element.tag    = xml.substr(pos + 1, nameEnd - pos - 1);
  element.tagEnd = findTagEnd(xml, nameEnd);
  if (element.tagEnd == NPOS || element.tag.empty())
    return false;
  if (xml[element.tagEnd - 2] == '/')
  {
    element.end = element.tagEnd;
    return true;
  }

  // the end tag of the element, elements with the same tag can be nested
  int    depth = 1;
  size_t p     = element.tagEnd;
  while (depth > 0)
  {
    p = findTag(xml, p);
    if (p == NPOS)
      return false;
    const bool   closing   = xml[p + 1] == '/';
    const size_t nameBegin = p + (closing ? 2 : 1);
    const size_t tagEnd    = findTagEnd(xml, p);
    if (tagEnd == NPOS)
      return false;
    const size_t after = nameBegin + element.tag.size();
    if (after < xml.size() && xml.compare(nameBegin, element.tag.size(), element.tag) == 0 && isNameEnd(xml[after]))
    {
      if (closing)
        depth--;
      else if (xml[tagEnd - 2] != '/')
        depth++;
    }
    p = tagEnd;
  }
  element.end = p;
  return true;
}

// calls f(element) for every element of the text, including the nested ones
template <typename FUNCTION_T>
void forEachElement(std::string_view xml, FUNCTION_T &&f)
{
  XmlElement element;
  size_t     pos = 0;
  while (nextElement(xml, pos, element))
  {
    f(element);
    pos = element.tagEnd;
  }
}

// the elements directly inside of an element
std::vector<XmlElement> children(std::string_view xml, const XmlElement &parent)
{
  std::vector<XmlElement> result;
  XmlElement              child;
  size_t                  pos = parent.tagEnd;
  while (pos < parent.end && nextElement(xml, pos, child) && child.begin < parent.end)
  {
    result.push_back(child);
    pos = child.end;
  }
  return result;
}

// the elements at document level: the children of the <materialx> element
std::vector<XmlElement> documentElements(std::string_view xml)
{
  XmlElement root;
  size_t     pos = 0;
  while (nextElement(xml, pos, root))
  {
    if (root.tag == "materialx")
      return children(xml, root);
    pos = root.tagEnd;
  }
  return {};
}

// position of an attribute of the start tag of an element
XmlAttribute findAttribute(std::string_view xml, const XmlElement &element, std::string_view name)
{
  const std::string_view tag = xml.substr(element.begin, element.tagEnd - element.begin);
  XmlAttribute           result;
  size_t                 pos = 0;
  while ((pos = tag.find(name, pos)) != NPOS)
  {
    size_t i = pos + name.size();
    if (pos > 0 && isSpace(tag[pos - 1]))
    {
      while (i < tag.size() && isSpace(tag[i]))
        i++;
      if (i < tag.size() && tag[i] == '=')
      {
        i++;
        while (i < tag.size() && isSpace(tag[i]))
          i++;
        if (i < tag.size() && (tag[i] == '"' || tag[i] == '\''))
        {
          const size_t close = tag.find(tag[i], i + 1);
          if (close != NPOS)
          {
            result.found      = true;
            result.begin      = element.begin + pos;
            result.end        = element.begin + close + 1;
            result.valueBegin = element.begin + i + 1;
            result.valueEnd   = element.begin + close;
            return result;
          }
        }
      }
    }
    pos += name.size();
  }
  return result;
}

bool hasAttribute(std::string_view xml, const XmlElement &element, std::string_view name)
{
  return findAttribute(xml, element, name).found;
}

// value of an attribute of the start tag of an element, empty if it has no such attribute
std::string attribute(std::string_view xml, const XmlElement &element, std::string_view name)
{
  const XmlAttribute a = findAttribute(xml, element, name);
  return a.found ? std::string(xml.substr(a.valueBegin, a.valueEnd - a.valueBegin)) : std::string();
}

// the input 'name' of a node, false if the node has no such input
bool findInput(std::string_view xml, const XmlElement &node, std::string_view name, XmlElement &input)
{
  for (const XmlElement &child : children(xml, node))
  {
    if ((child.tag == "input" || child.tag == "parameter") && attribute(xml, child, "name") == name)
    {
      input = child;
      return true;
    }
  }
  return false;
}

// value of the input 'name' of a node, 'defaultValue' if the node has no such input
std::string inputValue(
    std::string_view xml, const XmlElement &node, std::string_view name, const std::string &defaultValue)
{
  XmlElement input;
  return findInput(xml, node, name, input) ? attribute(xml, input, "value") : defaultValue;
}

// The attributes which define where the value of an input comes from (" value=..." or
// " nodename=... output=..."), to be used for another input with the same value.
std::string inputSource(std::string_view xml, const XmlElement &input)
{
  std::string result;
  for (const char *name : {"value", "nodename", "nodegraph", "output", "interfacename", "channels"})
  {
    const XmlAttribute a = findAttribute(xml, input, name);
    if (a.found)
      result += " " + std::string(xml.substr(a.begin, a.end - a.begin));
  }
  return result;
}

// applies edits which do not overlap
std::string applyEdits(const std::string &source, std::vector<Edit> &edits)
{
  if (edits.empty())
    return source;
  // from the end of the text to its start, so the positions of the remaining edits stay valid
  std::stable_sort(edits.begin(), edits.end(), [](const Edit &a, const Edit &b) { return a.begin > b.begin; });
  std::string result = source;
  for (const Edit &edit : edits)
    result.replace(edit.begin, edit.end - edit.begin, edit.text);
  return result;
}

// Rewriting passes /////////////////////////////////////////////////////////////

// Connects the inputs which refer to a document-level output to the node of the output and removes the
// outputs.
std::string resolveDocumentOutputs(const std::string &source)
{
  const std::string_view        xml      = source;
  const std::vector<XmlElement> elements = documentElements(xml);

  bool hasMaterial = false;
  for (const XmlElement &element : elements)
    hasMaterial |= element.tag == "surfacematerial";

  // name of the output -> the attributes of an input which is connected to the same node
  std::map<std::string, std::string> outputs;
  std::vector<Edit>                  edits;
  for (const XmlElement &element : elements)
  {
    if (element.tag != "output")
      continue;
    // without surfacematerial an output can be the result of the document
    const std::string type = attribute(xml, element, "type");
    if (!hasMaterial && (type == "surfaceshader" || type == "material"))
      continue;
    std::string connection;
    for (const char *name : {"nodename", "nodegraph", "output"})
    {
      if (hasAttribute(xml, element, name))
        connection += std::string(connection.empty() ? "" : " ") + name + "=\"" + attribute(xml, element, name) + "\"";
    }
    if (!connection.empty())
      outputs[attribute(xml, element, "name")] = connection;
    edits.push_back({element.begin, element.end, ""});
  }
  if (edits.empty())
    return source;

  for (const XmlElement &element : elements)
  {
    if (element.tag == "output" || element.tag == "nodegraph" || element.tag == "nodedef")
      continue;
    for (const XmlElement &input : children(xml, element))
    {
      if (input.tag != "input" || hasAttribute(xml, input, "nodename") || hasAttribute(xml, input, "nodegraph"))
        continue;
      const XmlAttribute output = findAttribute(xml, input, "output");
      if (!output.found)
        continue;
      const auto found = outputs.find(std::string(xml.substr(output.valueBegin, output.valueEnd - output.valueBegin)));
      if (found != outputs.end())
        edits.push_back({output.begin, output.end, found->second});
    }
  }
  return applyEdits(source, edits);
}

// Replaces <separate2>, <separate3> and <separate4> nodes, which the loader does not know, by one
// <extract> node per output which is used.
std::string replaceSeparate(const std::string &source)
{
  const std::string_view        xml      = source;
  const std::vector<XmlElement> elements = documentElements(xml);

  static const std::map<std::string, int> channels = {
      {"outx", 0}, {"outy", 1}, {"outz", 2}, {"outw", 3}, {"outr", 0}, {"outg", 1}, {"outb", 2}, {"outa", 3}};

  struct Separate
  {
    XmlElement            node;
    std::set<std::string> used; // names of the outputs
    bool                  replaceable{true};
  };
  std::map<std::string, Separate> separates;
  for (const XmlElement &element : elements)
  {
    if (element.tag == "separate2" || element.tag == "separate3" || element.tag == "separate4")
      separates[attribute(xml, element, "name")].node = element;
  }
  if (separates.empty())
    return source;

  // the inputs which are connected to an output of a separate node
  struct Connection
  {
    XmlAttribute nodeName;
    XmlAttribute output;
    std::string  name;
  };
  std::vector<Connection> connections;
  for (const XmlElement &element : elements)
  {
    if (element.tag == "nodegraph" || element.tag == "nodedef")
      continue;
    for (const XmlElement &input : children(xml, element))
    {
      const XmlAttribute nodeName = findAttribute(xml, input, "nodename");
      if (!nodeName.found)
        continue;
      const std::string name(xml.substr(nodeName.valueBegin, nodeName.valueEnd - nodeName.valueBegin));
      const auto        found = separates.find(name);
      if (found == separates.end())
        continue;
      const XmlAttribute output = findAttribute(xml, input, "output");
      const std::string  channel =
          output.found ? std::string(xml.substr(output.valueBegin, output.valueEnd - output.valueBegin)) : "";
      if (channels.count(channel) == 0)
      {
        found->second.replaceable = false; // the loader reports the node as unknown
        continue;
      }
      found->second.used.insert(channel);
      connections.push_back({nodeName, output, name});
    }
  }

  std::vector<Edit> edits;
  for (const Connection &connection : connections)
  {
    if (!separates[connection.name].replaceable)
      continue;
    const std::string channel(
        xml.substr(connection.output.valueBegin, connection.output.valueEnd - connection.output.valueBegin));
    edits.push_back({connection.nodeName.valueBegin, connection.nodeName.valueEnd, connection.name + GENERATED + channel});
    edits.push_back({connection.output.begin, connection.output.end, ""});
  }
  for (const auto &[name, separate] : separates)
  {
    XmlElement in;
    if (!separate.replaceable || !findInput(xml, separate.node, "in", in))
      continue;
    std::string text;
    for (const std::string &channel : separate.used)
    {
      text += "<extract name=\"" + name + GENERATED + channel + "\" type=\"float\"><input name=\"in\" type=\""
          + attribute(xml, in, "type") + "\"" + inputSource(xml, in) + " /><input name=\"index\" type=\"integer\" value=\""
          + std::to_string(channels.at(channel)) + "\" /></extract>";
    }
    edits.push_back({separate.node.begin, separate.node.end, text});
  }
  return applyEdits(source, edits);
}

// Removes all surface materials but one and replaces the geometry nodes which the loader does not
// support.
std::string selectMaterialAndGeometry(
    const std::string &source, const std::string &materialName, std::vector<std::string> &warnings)
{
  const std::string_view  xml = source;
  std::vector<Edit>       edits;
  std::vector<XmlElement> materials;

  forEachElement(xml, [&](const XmlElement &element) {
    if (element.tag == "surfacematerial")
    {
      materials.push_back(element);
    } else if (element.tag == "texcoord" || element.tag == "geomcolor")
    {
      const bool        isColor = element.tag == "geomcolor";
      const std::string index   = inputValue(xml, element, "index", "0");
      std::string       geomprop;
      if (isColor && index == "0")
        geomprop = "color";
      else if (!isColor && (index == "1" || index == "2" || index == "3"))
        geomprop = "attribute" + index;
      if (geomprop.empty())
        return;
      edits.push_back({element.begin, element.end,
          "<geompropvalue name=\"" + attribute(xml, element, "name") + "\" type=\"" + attribute(xml, element, "type")
              + "\"><input name=\"geomprop\" type=\"string\" value=\"" + geomprop + "\" /></geompropvalue>"});
    }
  });

  if (!materials.empty())
  {
    size_t selected = 0;
    if (!materialName.empty())
    {
      selected = materials.size();
      for (size_t i = 0; i < materials.size() && selected == materials.size(); i++)
      {
        if (attribute(xml, materials[i], "name") == materialName)
          selected = i;
      }
      if (selected == materials.size())
      {
        selected = 0;
        warnings.push_back("the document has no surfacematerial '" + materialName + "', '"
            + attribute(xml, materials[0], "name") + "' is used instead");
      }
    }
    for (size_t i = 0; i < materials.size(); i++)
    {
      if (i != selected)
        edits.push_back({materials[i].begin, materials[i].end, ""});
    }
  }
  return applyEdits(source, edits);
}

// Replaces <mix>, <add> and <multiply> nodes of the type EDF by a <uniform_edf> node whose color is
// calculated by a node of the same kind from the colors of the operands. This is exact as long as all
// operands are uniform EDFs.
std::string flattenEmission(const std::string &source, std::vector<std::string> &warnings)
{
  const std::string_view        xml      = source;
  const std::vector<XmlElement> elements = documentElements(xml);

  std::map<std::string, XmlElement> edfs; // EDF nodes by name
  bool                              needed = false;
  for (const XmlElement &element : elements)
  {
    if (attribute(xml, element, "type") != "EDF" || element.tag == "output")
      continue;
    edfs[attribute(xml, element, "name")] = element;
    needed |= element.tag == "mix" || element.tag == "add";
  }
  if (!needed)
    return source;

  auto isOperator = [](const XmlElement &e) { return e.tag == "mix" || e.tag == "add" || e.tag == "multiply"; };
  // names of the EDF inputs of an operator
  auto operands = [](const XmlElement &e) -> std::vector<const char *> {
    if (e.tag == "mix")
      return {"bg", "fg"};
    if (e.tag == "add")
      return {"in1", "in2"};
    return {"in1"};
  };

  // a node can be replaced if all EDFs it depends on are uniform EDFs or operators which can be replaced
  std::map<std::string, bool> convertible;
  std::set<std::string>       visiting;
  auto                        isConvertible = [&](const std::string &name, auto &&self) -> bool {
    const auto known = convertible.find(name);
    if (known != convertible.end())
      return known->second;
    const auto found = edfs.find(name);
    bool       result = false;
    if (found != edfs.end() && visiting.insert(name).second)
    {
      const XmlElement &node = found->second;
      if (node.tag == "uniform_edf")
      {
        result = true;
      } else if (isOperator(node))
      {
        result = true;
        for (const char *operand : operands(node))
        {
          XmlElement input;
          if (!findInput(xml, node, operand, input))
            continue; // no emission
          result = result && hasAttribute(xml, input, "nodename") && !hasAttribute(xml, input, "nodegraph")
              && self(attribute(xml, input, "nodename"), self);
        }
      }
      visiting.erase(name);
    }
    convertible[name] = result;
    return result;
  };

  // the attributes of a color3 input which gets the color of an EDF input of an operator
  auto colorSource = [&](const XmlElement &node, const char *operand) -> std::string {
    XmlElement input;
    if (!findInput(xml, node, operand, input))
      return " value=\"0, 0, 0\"";
    const std::string name = attribute(xml, input, "nodename");
    const XmlElement &edf  = edfs.at(name);
    if (edf.tag != "uniform_edf")
      return " nodename=\"" + name + GENERATED + "color\"";
    XmlElement color;
    return findInput(xml, edf, "color", color) ? inputSource(xml, color) : std::string(" value=\"1, 1, 1\"");
  };

  std::vector<Edit> edits;
  for (const auto &[name, node] : edfs)
  {
    if (!isOperator(node))
      continue;
    if (!isConvertible(name, isConvertible))
    {
      if (node.tag != "multiply")
      {
        warnings.push_back("the <" + std::string(node.tag) + "> node '" + name
            + "' combines EDFs which are not uniform, RadeonProRender does not render it correctly");
      }
      continue;
    }

    const std::string colorName = name + GENERATED + "color";
    std::string       text      = "<" + std::string(node.tag) + " name=\"" + colorName + "\" type=\"color3\">";
    for (const char *operand : operands(node))
      text += "<input name=\"" + std::string(operand) + "\" type=\"color3\"" + colorSource(node, operand) + " />";
    XmlElement input;
    if (node.tag == "mix")
    {
      text += "<input name=\"mix\" type=\"float\""
          + (findInput(xml, node, "mix", input) ? inputSource(xml, input) : std::string(" value=\"0\"")) + " />";
    } else if (node.tag == "multiply")
    {
      // the factor is a float or a color
      if (findInput(xml, node, "in2", input))
        text += "<input name=\"in2\" type=\"" + attribute(xml, input, "type") + "\"" + inputSource(xml, input) + " />";
      else
        text += "<input name=\"in2\" type=\"float\" value=\"1\" />";
    }
    text += "</" + std::string(node.tag) + "><uniform_edf name=\"" + name
        + "\" type=\"EDF\"><input name=\"color\" type=\"color3\" nodename=\"" + colorName + "\" /></uniform_edf>";
    edits.push_back({node.begin, node.end, text});
  }
  return applyEdits(source, edits);
}

// The 'roughness' input of the BSDF nodes is the squared roughness (alpha) in MaterialX. The loader
// squares it again, because its <roughness_anisotropy> node returns the roughness as it is. A roughness
// which does not come from a <roughness_anisotropy> node is replaced by its square root.
std::string adaptBsdfRoughness(const std::string &source)
{
  const std::string_view        xml      = source;
  const std::vector<XmlElement> elements = documentElements(xml);

  std::map<std::string, std::string_view> tags; // tag of the nodes by name
  for (const XmlElement &element : elements)
    tags[attribute(xml, element, "name")] = element.tag;

  std::vector<Edit> edits;
  for (const XmlElement &node : elements)
  {
    const bool isBsdf = node.tag.size() > 5 && node.tag.substr(node.tag.size() - 5) == "_bsdf";
    XmlElement roughness;
    if (!isBsdf || !findInput(xml, node, "roughness", roughness) || attribute(xml, roughness, "type") != "vector2")
      continue;

    const XmlAttribute value = findAttribute(xml, roughness, "value");
    if (value.found)
    {
      // constant: "x, y"
      const std::string text(xml.substr(value.valueBegin, value.valueEnd - value.valueBegin));
      const size_t      comma = text.find(',');
      const float       x     = parseFloat(text.substr(0, comma), 0.f);
      const float       y     = comma == std::string::npos ? x : parseFloat(text.substr(comma + 1), x);
      edits.push_back({value.valueBegin, value.valueEnd,
          formatFloat(std::sqrt(std::max(x, 0.f))) + ", " + formatFloat(std::sqrt(std::max(y, 0.f)))});
      continue;
    }

    const XmlAttribute nodeName = findAttribute(xml, roughness, "nodename");
    if (!nodeName.found || hasAttribute(xml, roughness, "nodegraph"))
      continue;
    const std::string name(xml.substr(nodeName.valueBegin, nodeName.valueEnd - nodeName.valueBegin));
    const auto        found = tags.find(name);
    if (found == tags.end() || found->second == "roughness_anisotropy")
      continue;
    const std::string sqrtName = attribute(xml, node, "name") + GENERATED + "roughness";
    edits.push_back({nodeName.valueBegin, nodeName.valueEnd, sqrtName});
    edits.push_back({node.begin, node.begin,
        "<sqrt name=\"" + sqrtName + "\" type=\"vector2\"><input name=\"in\" type=\"vector2\" nodename=\"" + name
            + "\"" + (hasAttribute(xml, roughness, "output") ? " output=\"" + attribute(xml, roughness, "output") + "\"" : "")
            + " /></sqrt>"});
  }
  return applyEdits(source, edits);
}

// Replaces the opacity of <surface> nodes: the BSDF is mixed with a lobe which transmits the light
// straight through and the EDF is multiplied by the opacity.
std::string emulateSurfaceOpacity(const std::string &source, std::vector<std::string> &warnings)
{
  const std::string_view xml = source;
  std::vector<Edit>      edits;

  for (const XmlElement &surface : documentElements(xml))
  {
    XmlElement opacity;
    if (surface.tag != "surface" || !findInput(xml, surface, "opacity", opacity))
      continue;
    const XmlAttribute value = findAttribute(xml, opacity, "value");
    if (value.found
        && parseFloat(std::string(xml.substr(value.valueBegin, value.valueEnd - value.valueBegin)), 0.f) == 1.f)
      continue; // opaque

    const std::string  name = attribute(xml, surface, "name");
    XmlElement         bsdf;
    const bool         hasBsdf      = findInput(xml, surface, "bsdf", bsdf);
    const XmlAttribute bsdfNodeName = hasBsdf ? findAttribute(xml, bsdf, "nodename") : XmlAttribute();
    if (!bsdfNodeName.found || hasAttribute(xml, bsdf, "nodegraph"))
    {
      warnings.push_back("the opacity of the <surface> node '" + name + "' is ignored");
      continue;
    }

    const std::string opacitySource = inputSource(xml, opacity);
    const std::string prefix        = name + GENERATED;
    std::string       nodes         = "<dielectric_bsdf name=\"" + prefix + "transparent\" type=\"BSDF\">"
        "<input name=\"tint\" type=\"color3\" value=\"1, 1, 1\" />"
        "<input name=\"ior\" type=\"float\" value=\"1\" />"
        "<input name=\"roughness\" type=\"vector2\" value=\"0, 0\" />"
        "<input name=\"scatter_mode\" type=\"string\" value=\"T\" />"
        "</dielectric_bsdf>"
        "<mix name=\"" + prefix + "bsdf\" type=\"BSDF\">"
        "<input name=\"fg\" type=\"BSDF\" nodename=\""
        + std::string(xml.substr(bsdfNodeName.valueBegin, bsdfNodeName.valueEnd - bsdfNodeName.valueBegin)) + "\" />"
        "<input name=\"bg\" type=\"BSDF\" nodename=\"" + prefix + "transparent\" />"
        "<input name=\"mix\" type=\"float\"" + opacitySource + " />"
        "</mix>";
    edits.push_back({bsdfNodeName.valueBegin, bsdfNodeName.valueEnd, prefix + "bsdf"});

    XmlElement edf;
    if (findInput(xml, surface, "edf", edf))
    {
      const XmlAttribute edfNodeName = findAttribute(xml, edf, "nodename");
      if (edfNodeName.found && !hasAttribute(xml, edf, "nodegraph"))
      {
        nodes += "<multiply name=\"" + prefix + "edf\" type=\"EDF\">"
            "<input name=\"in1\" type=\"EDF\" nodename=\""
            + std::string(xml.substr(edfNodeName.valueBegin, edfNodeName.valueEnd - edfNodeName.valueBegin)) + "\" />"
            "<input name=\"in2\" type=\"float\"" + opacitySource + " />"
            "</multiply>";
        edits.push_back({edfNodeName.valueBegin, edfNodeName.valueEnd, prefix + "edf"});
      }
    }

    edits.push_back({opacity.begin, opacity.end, ""});
    edits.push_back({surface.begin, surface.begin, nodes});
  }
  return applyEdits(source, edits);
}

} // namespace

std::vector<std::string> materialXImageFileNames(std::string_view xml)
{
  std::vector<std::string> names;
  forEachElement(xml, [&](const XmlElement &element) {
    if ((element.tag != "input" && element.tag != "parameter") || attribute(xml, element, "type") != "filename")
      return;
    std::string name = attribute(xml, element, "value");
    if (!name.empty() && std::find(names.begin(), names.end(), name) == names.end())
      names.push_back(std::move(name));
  });
  return names;
}

std::string prepareMaterialXDocument(
    const std::string &source, const std::string &materialName, std::vector<std::string> &warnings)
{
  // every pass works on the result of the previous one
  std::string document = resolveDocumentOutputs(source);
  document             = replaceSeparate(document);
  document             = selectMaterialAndGeometry(document, materialName, warnings);
  document             = flattenEmission(document, warnings);
  document             = adaptBsdfRoughness(document);
  document             = emulateSurfaceOpacity(document, warnings);
  return document;
}

} // namespace anari::rpr

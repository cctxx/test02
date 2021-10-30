using System.Text;
using System.Xml;

namespace UnityEditor
{
internal class AndroidXmlDocument : XmlDocument
{
	private string m_Path;

	public AndroidXmlDocument(string path)
	{
		m_Path = path;
		var reader = new XmlTextReader(m_Path);
		try
		{
			reader.Read();
			Load(reader);
		}
		finally
		{
			reader.Close();
		}
	}

	public string Save()
	{
		return SaveAs(m_Path);
	}

	public string SaveAs(string path)
	{
		var writer = new XmlTextWriter(path, new UTF8Encoding(false));
		try
		{
			writer.Formatting = Formatting.Indented;
			Save(writer);
		}
		finally
		{
			writer.Close();
		}
		return path;
	}

	public XmlAttribute CreateAttribute(string prefix, string localName, string namezpace, string value)
	{
		XmlAttribute attr = CreateAttribute(prefix, localName, namezpace);
		attr.Value = value;
		return attr;
	}

	public XmlElement GetElementWithAttribute(XmlElement node, string tag, string attribute, string attributeValue)
	{
		foreach (var childNode in node.ChildNodes)
			if (childNode is XmlElement)
				if (((XmlElement)childNode).Name == tag)
					if (((XmlElement)childNode).GetAttribute(attribute) == attributeValue)
						return (XmlElement)childNode;
		return null;
	}

	public XmlElement GetElementWithAttribute(XmlElement node, string tag, string attribute)
	{
		foreach (var childNode in node.ChildNodes)
			if (childNode is XmlElement)
				if (((XmlElement)childNode).Name == tag)
					if (((XmlElement)childNode).HasAttribute(attribute))
						return (XmlElement)childNode;
		return null;
	}

	protected XmlElement AppendElement(XmlElement node, string tag, string attribute)
	{
		if (GetElementWithAttribute(node, tag, attribute) != null)
			return null;
		return (XmlElement)node.AppendChild(CreateElement(tag));
	}

	protected XmlElement AppendElement(XmlElement node, string tag, string attribute, string attributeValue)
	{
		if (GetElementWithAttribute(node, tag, attribute, attributeValue) != null)
			return null;
		return (XmlElement)node.AppendChild(CreateElement(tag));
	}
}
}
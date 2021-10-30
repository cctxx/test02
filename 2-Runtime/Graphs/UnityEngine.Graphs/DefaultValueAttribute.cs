using System;

namespace UnityEngine.Graphs.LogicGraph
{
	[AttributeUsage(AttributeTargets.All)]
	public class DefaultValueAttribute : Attribute
	{
		private object m_Value;
		private Type m_Type;

		public object value
		{
			get { return m_Value; }
		}

		public Type type
		{
			get { return m_Type; }
		}

		public DefaultValueAttribute(object value) 
		{ 
			m_Value = value; 
		}

		public DefaultValueAttribute(Type type, string value)
		{
			m_Value = value;
			m_Type = type;
		}
	}
}


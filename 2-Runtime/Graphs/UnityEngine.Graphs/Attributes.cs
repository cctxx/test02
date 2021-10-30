using System;

namespace UnityEngine.Graphs.LogicGraph
{
	[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property | AttributeTargets.Parameter)]
	public class SettingAttribute : Attribute { }

	// TODO : should be abstract
	// for almost everything logic graph related (classes, functions, variables, ...)
	[AttributeUsage(AttributeTargets.Class | AttributeTargets.Method | AttributeTargets.Event | AttributeTargets.Field | AttributeTargets.Property)]
	public class CodeGeneratingLogicAttribute : Attribute
	{
		public Type type;
		public Type inputsType;
		public Type stateType;
	}

	// for almost everything logic graph related (classes, functions, variables, ...)
	public class LogicAttribute : CodeGeneratingLogicAttribute
	{
		public LogicAttribute() { type = null; }
		public LogicAttribute(Type type) { this.type = type; }
		public LogicAttribute(Type type, Type inputsType)
		{
			base.type = type;
			base.inputsType = inputsType;
		}
		public LogicAttribute(Type type, Type inputsType, Type stateType)
		{
			base.type = type;
			base.inputsType = inputsType;
			base.stateType = stateType;
		}
	}

	// for evaluator nodes
	[AttributeUsage(AttributeTargets.Method | AttributeTargets.Class)]
	public class LogicEvalAttribute : Attribute
	{
		public Type type;
		public Type stateType;

		public LogicEvalAttribute() { type = null; }
		public LogicEvalAttribute(Type type) { this.type = type; }
		public LogicEvalAttribute(Type type, Type stateType)
		{
			this.type = type;
			this.stateType = stateType;
		}
	}

	[AttributeUsage(AttributeTargets.Method | AttributeTargets.Property | AttributeTargets.Field)]
	public class LogicExpressionAttribute: Attribute
	{
		public string name;
		public LogicExpressionAttribute() { name = string.Empty;}
		public LogicExpressionAttribute(string name) { this.name = name; }
	}

	[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
	public class LogicTargetAttribute : Attribute { }

	[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property | AttributeTargets.Method | AttributeTargets.Class | AttributeTargets.Event | AttributeTargets.Parameter | AttributeTargets.ReturnValue)]
	public class TitleAttribute : Attribute
	{
		public string title;
		public TitleAttribute(string title) { this.title = title; }		
	}

	[AttributeUsage(AttributeTargets.Class | AttributeTargets.Method | AttributeTargets.Event | AttributeTargets.Field | AttributeTargets.Property)]
	public class ValidateAttribute : Attribute
	{
		public string validateFunction;
		public ValidateAttribute(string validateFunction) { this.validateFunction = validateFunction; }
	}
	
	#if UNITY_ANIMATION_GRAPH
	// for almost everything logic graph related (classes, functions, variables, ...)
	public class AnimationLogicAttribute : CodeGeneratingLogicAttribute
	{
		public AnimationLogicAttribute() { type = null; }
		public AnimationLogicAttribute(Type type) { base.type = type; }
		public AnimationLogicAttribute(Type type, Type inputsType)
		{
			base.type = type;
			base.inputsType = inputsType;
		}
		public AnimationLogicAttribute(Type type, Type inputsType, Type stateType)
		{
			base.type = type;
			base.inputsType = inputsType;
			base.stateType = stateType;
		}
	}
	#endif
}

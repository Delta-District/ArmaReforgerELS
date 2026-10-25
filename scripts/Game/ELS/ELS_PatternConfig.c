
[BaseContainerProps(configRoot: true)]
class DD_EmergencyPatternSet : ScriptAndConfig {
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Pattern assets", params: "conf")]
	ref array<ResourceName> PatternDefs;
}

[BaseContainerProps(configRoot: true)]
class DD_EmergencyPatternDef : ScriptAndConfig {
	[Attribute(defvalue: "Pattern", desc: "Display name")]
	string Name;

	[Attribute(desc: "Ordered steps for this pattern")]
	ref array<ref DD_EmergencyPatternStep> Steps;
}

[BaseContainerProps()]
class DD_EmergencyPatternStep : ScriptAndConfig {
	[Attribute(defvalue: "0", desc: "Bitmask for ELS IDs (bit0=ID1 ... bit23=ID24)")]
	int Mask;

	[Attribute(defvalue: "1.0", desc: "Duration multiplier (Only use for very specific patterns - Higher = Longer)")]
	float Duration;
}

[BaseContainerProps()]
class DD_EmergencyPatternPause : DD_EmergencyPatternStep {
	void DD_EmergencyPatternPause() {
		Mask = 0;
		Duration = 1.0;
	}
}
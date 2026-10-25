
[ComponentEditorProps(category: "DeltaDistrict|ELS", description: "Light slot with ELS ID")]
class ELS_LightSlot : SCR_LightSlot {
	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox, desc: "ELS ID", category: "ELS")]
	protected int m_iELS_ID;
	int GetELS_ID() { return m_iELS_ID; }
}

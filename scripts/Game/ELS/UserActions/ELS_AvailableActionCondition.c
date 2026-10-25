[BaseContainerProps()]
class ELS_AvailableCondition : SCR_AvailableActionCondition
{
	[Attribute("1", UIWidgets.Auto, "If true, require vehicle to have ELS lights turned on")]
	protected bool m_bRequireLightsOn;
	
	[Attribute("0", UIWidgets.Auto, "If true, require PRIMARY group has at least one light")]
	protected bool m_bRequirePrimaryLights;

	[Attribute("0", UIWidgets.Auto, "If true, require SECONDARY group has at least one light")]
	protected bool m_bRequireSecondaryLights;

	[Attribute("0", UIWidgets.Auto, "If true, require WARNING group has at least one light")]
	protected bool m_bRequireWarningLights;

	[Attribute("0", UIWidgets.Auto, "If true, require ACCESSORY group has at least one light")]
	protected bool m_bRequireAccessoryLights;
	
	[Attribute("1", UIWidgets.Auto, "If true, requires HUD to be active to display")]
	protected bool m_bRequireActiveHud;

	override bool IsAvailable(notnull  SCR_AvailableActionsConditionData data) {
		IEntity vehicleEnt = data.GetCurrentVehicle();
		if (!vehicleEnt) return GetReturnResult(false);
		ELS_EmergencyLightController els = ELS_EmergencyLightController.Cast(vehicleEnt.FindComponent(ELS_EmergencyLightController));
		if (!els) return GetReturnResult(false);
		if (m_bRequireActiveHud && els.IsHudHidden()) return GetReturnResult(false);
		if (m_bRequireLightsOn && els.GetStage() <= 0) return GetReturnResult(false);
		if (m_bRequirePrimaryLights && !els.HasGroupLights(DD_ELS_GROUP_PRIMARY)) return GetReturnResult(false);
		if (m_bRequireSecondaryLights && !els.HasGroupLights(DD_ELS_GROUP_SECONDARY)) return GetReturnResult(false);
		if (m_bRequireWarningLights && !els.HasGroupLights(DD_ELS_GROUP_WARNING)) return GetReturnResult(false);
		if (m_bRequireAccessoryLights && !els.HasGroupLights(DD_ELS_GROUP_ACCESSORY))return GetReturnResult(false);
		return GetReturnResult(true);
	}
}
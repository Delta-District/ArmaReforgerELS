class DD_AutoEmergencyLightsComponentClass : ScriptComponentClass {}

class DD_AutoEmergencyLightsComponent : ScriptComponent
{
	[Attribute("300", UIWidgets.EditBox, desc: "Seconds to keep lights on (0 disables auto-off)")]
	protected float m_fDurationS;

	[Attribute("1", UIWidgets.CheckBox, desc: "Turn lights on immediately on spawn")]
	protected bool m_bEnableOnSpawn;

	protected IEntity m_Owner;
	protected ELS_EmergencyLightController ctrl;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_Owner = owner;
		if (!Replication.IsServer()) return;
		GetGame().GetCallqueue().CallLater(ApplyOnSpawn, 0, false);
	}

	protected void ApplyOnSpawn()
	{
		if (!m_Owner) return;
		if (m_bEnableOnSpawn) ActivateEmergencyLights();
		if (m_fDurationS > 0) {
			int ms = Math.Round(m_fDurationS * 1000);
			GetGame().GetCallqueue().CallLater(DisableEmergencyLights, ms, false);
		}
	}

	protected void ActivateEmergencyLights()
	{
		if (!m_Owner) return;
		ctrl = ELS_EmergencyLightController.Cast(m_Owner.FindComponent(ELS_EmergencyLightController));
		if (ctrl) ctrl.RequestEnableELS(3);
	}
	
	protected void DisableEmergencyLights()
	{
		if (!m_Owner) return;
		if (!ctrl) ctrl = ELS_EmergencyLightController.Cast(m_Owner.FindComponent(ELS_EmergencyLightController));
		if (ctrl) ctrl.RequestDisableELS();
	}
}

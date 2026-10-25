
class DD_ELS_ActionUtil
{
	static IEntity ResolveVehicleOwner(IEntity owner) {
		IEntity o = owner;
		IEntity foundCtrl;
		while (o) {
			if (o.FindComponent(ELS_EmergencyLightController)) foundCtrl = o;
			if (o.FindComponent(BaseLightManagerComponent) || o.FindComponent(SCR_VehicleDamageManagerComponent) || o.FindComponent(SCR_WheeledDamageManagerComponent)) {
				if (o.FindComponent(ELS_EmergencyLightController)) return o;
				break;
			}
			o = o.GetParent();
		}
		return foundCtrl;
	}

	static ELS_EmergencyLightController GetController(ScriptedUserAction action, IEntity pOwnerEntity) {
		IEntity target = pOwnerEntity;
		if (!target || !target.FindComponent(ELS_EmergencyLightController)) target = ResolveVehicleOwner(action.GetOwner());
		if (!target) return null;
		return ELS_EmergencyLightController.Cast(target.FindComponent(ELS_EmergencyLightController));
	}
	
	static void PlaySound(IEntity ownerEntity) {
		SoundComponent soundComponent = SoundComponent.Cast(ownerEntity.FindComponent(SoundComponent));
		if (!soundComponent) return;
		soundComponent.SoundEvent(SCR_SoundEvent.SOUND_VEHICLE_CLOSE_LIGHT_ON);
	}
}


// ------------------------------------------------------------
// ELS Toggle Action
// ------------------------------------------------------------
class DD_ActionToggleELS : ScriptedUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) {
		if (pUserEntity != SCR_PlayerController.GetLocalControlledEntity()) return;
		ELS_EmergencyLightController ctrl = DD_ELS_ActionUtil.GetController(this, pOwnerEntity);
		if (!ctrl) return;
		ctrl.RequestToggleELS();
		if (!ctrl.IsHudHidden()) SCR_HintManagerComponent.ShowCustomHint(string.Format("ELS: %1", ctrl.GetStageLabel()), "Emergency Lights", 2.0, true);
	}
	override bool CanBeShownScript(IEntity user) {
		return DD_ELS_ActionUtil.ResolveVehicleOwner(GetOwner()) != null;
	}
	override bool CanBePerformedScript(IEntity user) { return true; }
	override bool HasLocalEffectOnlyScript() { return false; }
}


// ------------------------------------------------------------
// ELS Stage Actions (Next / Previous)
// ------------------------------------------------------------
class DD_ActionNextELSStage : ScriptedUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) {
		if (pUserEntity != SCR_PlayerController.GetLocalControlledEntity()) return;
		ELS_EmergencyLightController ctrl = DD_ELS_ActionUtil.GetController(this, pOwnerEntity);
		if (!ctrl) return;
		ctrl.RequestNextStage();
		DD_ELS_ActionUtil.PlaySound(pOwnerEntity);
		if (!ctrl.IsHudHidden()) SCR_HintManagerComponent.ShowCustomHint(string.Format("ELS: %1", ctrl.GetStageLabel()), "Emergency Lights", 2.0, true);
	}
	override bool CanBeShownScript(IEntity user) {
		return DD_ELS_ActionUtil.ResolveVehicleOwner(GetOwner()) != null;
	}
	override bool CanBePerformedScript(IEntity user) { return true; }
	override bool HasLocalEffectOnlyScript() { return false; }
}

class DD_ActionPrevELSStage : ScriptedUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) {
		if (pUserEntity != SCR_PlayerController.GetLocalControlledEntity()) return;
		ELS_EmergencyLightController ctrl = DD_ELS_ActionUtil.GetController(this, pOwnerEntity);
		if (!ctrl) return;
		ctrl.RequestPrevStage();
		DD_ELS_ActionUtil.PlaySound(pOwnerEntity);
		if (!ctrl.IsHudHidden()) SCR_HintManagerComponent.ShowCustomHint(string.Format("ELS: %1", ctrl.GetStageLabel()), "Emergency Lights", 2.0, true);
	}
	override bool CanBeShownScript(IEntity user) {
		return DD_ELS_ActionUtil.ResolveVehicleOwner(GetOwner()) != null;
	}
	override bool CanBePerformedScript(IEntity user) { return true; }
	override bool HasLocalEffectOnlyScript() { return false; }
}


class DD_ActionELSFlashSettings : ScriptedUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) {
		IEntity vehicle = pOwnerEntity;
		if (pUserEntity != SCR_PlayerController.GetLocalControlledEntity()) return;
		if (!vehicle || !vehicle.FindComponent(ELS_EmergencyLightController)) vehicle = DD_ELS_ActionUtil.ResolveVehicleOwner(pOwnerEntity);
		ELS_EmergencyLightController ctrl = ELS_EmergencyLightController.Cast(vehicle.FindComponent(ELS_EmergencyLightController));
		if (!ctrl) return;
		ctrl.ToggleFlashMenuFor(pUserEntity);
		DD_ELS_ActionUtil.PlaySound(pOwnerEntity);
	}
	override bool CanBeShownScript(IEntity user) { return DD_ELS_ActionUtil.ResolveVehicleOwner(GetOwner()) != null; }
	override bool CanBePerformedScript(IEntity user) { return true; }
}

class DD_ActionELSPatternControl : ScriptedUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) {
		IEntity vehicle = pOwnerEntity;
		if (pUserEntity != SCR_PlayerController.GetLocalControlledEntity()) return;
		if (!vehicle || !vehicle.FindComponent(ELS_EmergencyLightController)) vehicle =  DD_ELS_ActionUtil.ResolveVehicleOwner(pOwnerEntity);
		ELS_EmergencyLightController ctrl = ELS_EmergencyLightController.Cast(vehicle.FindComponent(ELS_EmergencyLightController));
		if (!ctrl) return;
		ctrl.TogglePatternMenuFor(pUserEntity);
		DD_ELS_ActionUtil.PlaySound(pOwnerEntity);
	}
	override bool CanBeShownScript(IEntity user) { return DD_ELS_ActionUtil.ResolveVehicleOwner(GetOwner()) != null; }
	override bool CanBePerformedScript(IEntity user) { return true; }
}


// ------------------------------------------------------------
// ELS Pattern Group Actions Base (Next / Previous)
// ------------------------------------------------------------
class DD_ActionELSPatternBase : ScriptedUserAction
{
	protected int m_iGroup;
	protected bool m_bNext; // true=Next, false=Previous
	override bool CanBeShownScript(IEntity user) {
		ELS_EmergencyLightController ctrl = DD_ELS_ActionUtil.GetController(this, GetOwner()); 
		if (!ctrl) return false;
		return ctrl.IsPatternMenuOpenFor(user);
	}
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) {
		if (pUserEntity != SCR_PlayerController.GetLocalControlledEntity()) return;
		ELS_EmergencyLightController ctrl = DD_ELS_ActionUtil.GetController(this, pOwnerEntity);
		if (!ctrl) return;
		if (ctrl.GetStage() == 0) return;
		if (m_bNext)
			ctrl.RequestNextPattern(m_iGroup);
		else
			ctrl.RequestPrevPattern(m_iGroup);
		ctrl.TouchPatternMenuFor(pUserEntity);
		DD_ELS_ActionUtil.PlaySound(pOwnerEntity);
		if (!ctrl.IsHudHidden()) SCR_HintManagerComponent.ShowCustomHint(string.Format("ELS Pattern: %1", ctrl.GetPatternLabel(m_iGroup)), "Emergency Lights", 2.0, true);
	}
	override bool CanBePerformedScript(IEntity user) { return true; }
	override bool HasLocalEffectOnlyScript() { return false; }
}

// Next Pattern
class DD_ActionNextELSPatternPrimary : DD_ActionELSPatternBase {
	void DD_ActionNextELSPatternPrimary() { m_iGroup = DD_ELS_GROUP_PRIMARY; m_bNext = true; }
}
class DD_ActionNextELSPatternSecondary : DD_ActionELSPatternBase {
	void DD_ActionNextELSPatternSecondary() { m_iGroup = DD_ELS_GROUP_SECONDARY; m_bNext = true; }
}
class DD_ActionNextELSPatternWarning : DD_ActionELSPatternBase {
	void DD_ActionNextELSPatternWarning() { m_iGroup = DD_ELS_GROUP_WARNING; m_bNext = true; }
}
class DD_ActionNextELSPatternAccessory : DD_ActionELSPatternBase {
	void DD_ActionNextELSPatternAccessory() { m_iGroup = DD_ELS_GROUP_ACCESSORY; m_bNext = true; }
}

// Previous Pattern
class DD_ActionPrevELSPatternPrimary : DD_ActionELSPatternBase {
	void DD_ActionPrevELSPatternPrimary() { m_iGroup = DD_ELS_GROUP_PRIMARY; m_bNext = false; }
}
class DD_ActionPrevELSPatternSecondary : DD_ActionELSPatternBase {
	void DD_ActionPrevELSPatternSecondary() { m_iGroup = DD_ELS_GROUP_SECONDARY; m_bNext = false; }
}
class DD_ActionPrevELSPatternWarning : DD_ActionELSPatternBase {
	void DD_ActionPrevELSPatternWarning() { m_iGroup = DD_ELS_GROUP_WARNING; m_bNext = false; }
}
class DD_ActionPrevELSPatternAccessory : DD_ActionELSPatternBase {
	void DD_ActionPrevELSPatternAccessory() { m_iGroup = DD_ELS_GROUP_ACCESSORY; m_bNext = false; }
}

// ------------------------------------------------------------
// Speed Actions (Next / Previous) with button feedback
// ------------------------------------------------------------
class DD_ActionNextELSFlashSpeed : ScriptedUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) {
		if (pUserEntity != SCR_PlayerController.GetLocalControlledEntity()) return;
		ELS_EmergencyLightController ctrl = DD_ELS_ActionUtil.GetController(this, pOwnerEntity);
		if (!ctrl) return;
		ctrl.RequestNextFlashSpeed();
		ctrl.TouchFlashMenuFor(pUserEntity);
		DD_ELS_ActionUtil.PlaySound(pOwnerEntity);
		if (!ctrl.IsHudHidden()) SCR_HintManagerComponent.ShowCustomHint(string.Format("ELS Flash Speed: %1", ctrl.GetFlashSpeedLabel()), "Emergency Lights", 2.0, true);
	}
	override bool CanBeShownScript(IEntity user) {
		ELS_EmergencyLightController ctrl = DD_ELS_ActionUtil.GetController(this, GetOwner()); 
		if (!ctrl) return false;
		return ctrl.IsFlashMenuOpenFor(user);
	}
	override bool CanBePerformedScript(IEntity user) { return true; }
}

class DD_ActionPrevELSFlashSpeed : ScriptedUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) {
		if (pUserEntity != SCR_PlayerController.GetLocalControlledEntity()) return;
		ELS_EmergencyLightController ctrl = DD_ELS_ActionUtil.GetController(this, pOwnerEntity);
		if (!ctrl) return;
		ctrl.RequestPrevFlashSpeed();
		ctrl.TouchFlashMenuFor(pUserEntity);
		DD_ELS_ActionUtil.PlaySound(pOwnerEntity);
		if (!ctrl.IsHudHidden()) SCR_HintManagerComponent.ShowCustomHint(string.Format("ELS Flash Speed: %1", ctrl.GetFlashSpeedLabel()), "Emergency Lights", 2.0, true);
	}
	override bool CanBeShownScript(IEntity user) {
		ELS_EmergencyLightController ctrl = DD_ELS_ActionUtil.GetController(this, GetOwner()); 
		if (!ctrl) return false;
		return ctrl.IsFlashMenuOpenFor(user);
	}
	override bool CanBePerformedScript(IEntity user) { return true; }
}

class DD_ActionNextELSPauseSpeed : ScriptedUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) {
		if (pUserEntity != SCR_PlayerController.GetLocalControlledEntity()) return;
		ELS_EmergencyLightController ctrl = DD_ELS_ActionUtil.GetController(this, pOwnerEntity);
		if (!ctrl) return;
		ctrl.RequestNextPauseSpeed();
		DD_ELS_ActionUtil.PlaySound(pOwnerEntity);
		if (!ctrl.IsHudHidden()) SCR_HintManagerComponent.ShowCustomHint(string.Format("ELS Pause Speed: %1", ctrl.GetPauseSpeedLabel()), "Emergency Lights", 2.0, true);
	}
	override bool CanBeShownScript(IEntity user) {
		ELS_EmergencyLightController ctrl = DD_ELS_ActionUtil.GetController(this, GetOwner()); 
		if (!ctrl) return false;
		return ctrl.IsFlashMenuOpenFor(user);
	}
	override bool CanBePerformedScript(IEntity user) { return true; }
}

class DD_ActionPrevELSPauseSpeed : ScriptedUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) {
		if (pUserEntity != SCR_PlayerController.GetLocalControlledEntity()) return;
		ELS_EmergencyLightController ctrl = DD_ELS_ActionUtil.GetController(this, pOwnerEntity);
		if (!ctrl) return;
		ctrl.RequestPrevPauseSpeed();
		DD_ELS_ActionUtil.PlaySound(pOwnerEntity);
		if (!ctrl.IsHudHidden()) SCR_HintManagerComponent.ShowCustomHint(string.Format("ELS Pause Speed: %1", ctrl.GetPauseSpeedLabel()), "Emergency Lights", 2.0, true);
	}
	override bool CanBeShownScript(IEntity user) {
		ELS_EmergencyLightController ctrl = DD_ELS_ActionUtil.GetController(this, GetOwner()); 
		if (!ctrl) return false;
		return ctrl.IsFlashMenuOpenFor(user);
	}
	override bool CanBePerformedScript(IEntity user) { return true; }
}


// ------------------------------------------------------------
//   Emergency Lights (MP-safe stage, pattern, and flash/pause duration replication, local timing)
// 		- If patterns exist, they drive ON/OFF using a bitmask where bit0 = ID 1, bit23 = ID 24
// ------------------------------------------------------------

static const int DD_ELS_GROUP_PRIMARY   = 0;
static const int DD_ELS_GROUP_SECONDARY = 1;
static const int DD_ELS_GROUP_WARNING   = 2;
static const int DD_ELS_GROUP_ACCESSORY = 3;

class ELS_EmergencyLightControllerClass : ScriptComponentClass {}

class ELS_EmergencyLightController : ScriptComponent
{
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Primary Pattern Config", params: "conf")]
	protected ResourceName m_PrimaryPatternSetRes;

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Secondary Pattern Config", params: "conf")]
	protected ResourceName m_SecondaryPatternSetRes;

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Warning Pattern Config", params: "conf")]
	protected ResourceName m_WarningPatternSetRes;

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Accessory Pattern Config", params: "conf")]
	protected ResourceName m_AccessoryPatternSetRes;

	[Attribute(desc: "Additional primary patterns defined on this prefab.")]
	protected ref array<ref DD_EmergencyPatternDef> m_PrefabPrimaryPatterns;

	[Attribute(desc: "Additional secondary patterns defined on this prefab.")]
	protected ref array<ref DD_EmergencyPatternDef> m_PrefabSecondaryPatterns;

	[Attribute(desc: "Additional warning patterns defined on this prefab.")]
	protected ref array<ref DD_EmergencyPatternDef> m_PrefabWarningPatterns;

	[Attribute(desc: "Additional accessory patterns defined on this prefab.")]
	protected ref array<ref DD_EmergencyPatternDef> m_PrefabAccessoryPatterns;

	[Attribute(defvalue: "1", desc: "Stage 1 enables WARNING group.")]
	protected bool m_bStage1Warning;

	[Attribute(defvalue: "0", desc: "Stage 1 enables ACCESSORY group.")]
	protected bool m_bStage1Accessory;

	[Attribute(defvalue: "1", desc: "Stage 2 enables WARNING group.")]
	protected bool m_bStage2Warning;

	[Attribute(defvalue: "1", desc: "Stage 2 enables SECONDARY group.")]
	protected bool m_bStage2Secondary;

	[Attribute(defvalue: "1", desc: "Stage 2 enables ACCESSORY group.")]
	protected bool m_bStage2Accessory;

	[Attribute(defvalue: "1", desc: "Stage 2 enables selected PRIMARY IDs (see Stage2PrimaryIDs).")]
	protected bool m_bStage2PrimaryPartial;

	[Attribute(desc: "Primary IDs enabled in Stage 2 if Stage2PrimaryPartial is true.")]
	protected ref array<int> m_Stage2PrimaryIDs;

	[Attribute(defvalue: "1", desc: "Stage 3 enables PRIMARY group.")]
	protected bool m_bStage3Primary;

	[Attribute(defvalue: "1", desc: "Stage 3 enables SECONDARY group.")]
	protected bool m_bStage3Secondary;

	[Attribute(defvalue: "1", desc: "Stage 3 enables ACCESSORY group.")]
	protected bool m_bStage3Accessory;

	[Attribute(defvalue: "0", desc: "Stage 3 enables WARNING group (usually off).")]
	protected bool m_bStage3Warning;

	// ELS IDs
	[Attribute(desc: "ELS IDs that belong to the PRIMARY group.")]
	protected ref array<int> m_PrimaryIDs;

	[Attribute(desc: "ELS IDs that belong to the SECONDARY group.")]
	protected ref array<int> m_SecondaryIDs;

	[Attribute(desc: "ELS IDs that belong to the WARNING group.")]
	protected ref array<int> m_WarningIDs;

	[Attribute(desc: "ELS IDs that belong to the ACCESSORY group.")]
	protected ref array<int> m_AccessoryIDs;

	[Attribute(defvalue: "0.1", desc: "Update interval (seconds).")]
	protected float m_fTick;
	
	// ------------------------------------------------------------
	// Runtime
	// ------------------------------------------------------------
	protected SCR_VehicleDamageManagerComponent m_VehDmg;
	protected BaseLightManagerComponent m_LightMgr;
	protected SoundComponent m_VehSound;
	
	protected InputManager m_Input;
	protected RplComponent m_Rpl;

	protected ref array<ELS_LightSlot> m_AllSlots = {};
	protected ref map<int, int> m_IdToGroup; // ELS_ID -> group

	// Runtime patterns per group
	protected ref array<ref DD_EmergencyPatternDef> m_PrimaryPatterns = {};
	protected ref array<ref DD_EmergencyPatternDef> m_SecondaryPatterns = {};
	protected ref array<ref DD_EmergencyPatternDef> m_WarningPatterns = {};
	protected ref array<ref DD_EmergencyPatternDef> m_AccessoryPatterns = {};
	
	// Sequencer state per group (local only)
	protected int m_PrimaryStepIndex;
	protected float m_PrimaryStepTimer;
	protected int m_SecondaryStepIndex;
	protected float m_SecondaryStepTimer;
	protected int m_WarningStepIndex;
	protected float m_WarningStepTimer;
	protected int m_AccessoryStepIndex;
	protected float m_AccessoryStepTimer;

	protected IEntity m_VehicleRoot;
	protected bool m_bDestroyed = false;
	protected bool m_HideHUD;
	protected bool m_bTicking;
	protected int m_iLastStage = 1;
	protected bool m_Bound;

	// ------------------------------------------------------------
	// Replicated state
	// ------------------------------------------------------------
	[RplProp(onRplName: "OnRplStageChanged")]
	protected int m_iStage; // 0..3

	[RplProp(onRplName: "OnRplPrimaryPatternChanged")]
	protected int m_iPrimaryPattern;

	[RplProp(onRplName: "OnRplSecondaryPatternChanged")]
	protected int m_iSecondaryPattern;

	[RplProp(onRplName: "OnRplWarningPatternChanged")]
	protected int m_iWarningPattern;

	[RplProp(onRplName: "OnRplAccessoryPatternChanged")]
	protected int m_iAccessoryPattern;

	// Stage-specific speeds packed into a single int each:
	// bits 0..1  = stage1 (0..2)
	// bits 2..3  = stage2 (0..2)
	// bits 4..5  = stage3 (0..2)
	[RplProp(onRplName: "OnRplFlashSpeedChanged")]
	protected int m_iFlashSpeedPacked = 0;

	[RplProp(onRplName: "OnRplPauseSpeedChanged")]
	protected int m_iPauseSpeedPacked = 0;
	
	
	// ------------------------------------------------------------
	// Init
	// ------------------------------------------------------------
	override void OnPostInit(IEntity owner) {
		super.OnPostInit(owner);
		m_VehicleRoot = ResolveVehicleRoot();
		if (!m_VehicleRoot) {
			Print("[ELS] Failed to resolve vehicle root", LogLevel.ERROR);
			return;
		}
		if (owner != m_VehicleRoot) {
			Print(string.Format("[ELS] Controller is on %1 but vehicle root is %2. Move ELS_EmergencyLightController to the replicated vehicle root entity.", owner, m_VehicleRoot), LogLevel.ERROR);
			return;
		}
		m_Rpl = RplComponent.Cast(m_VehicleRoot.FindComponent(RplComponent));
		if (!m_Rpl) {
			Print(string.Format("[ELS] No RplComponent found on controller owner/root %1", owner), LogLevel.ERROR);
			return;
		}
		m_LightMgr = BaseLightManagerComponent.Cast(owner.FindComponent(BaseLightManagerComponent));
		m_VehSound = SoundComponent.Cast(owner.FindComponent(SoundComponent));
		EnsureArrays();
		BuildSlots();
		BuildIdGroupMap();
		LoadPatternsForGroup(DD_ELS_GROUP_PRIMARY,   m_PrimaryPatternSetRes,   m_PrimaryPatterns,   m_PrefabPrimaryPatterns);
		LoadPatternsForGroup(DD_ELS_GROUP_SECONDARY, m_SecondaryPatternSetRes, m_SecondaryPatterns, m_PrefabSecondaryPatterns);
		LoadPatternsForGroup(DD_ELS_GROUP_WARNING,   m_WarningPatternSetRes,   m_WarningPatterns,   m_PrefabWarningPatterns);
		LoadPatternsForGroup(DD_ELS_GROUP_ACCESSORY, m_AccessoryPatternSetRes, m_AccessoryPatterns, m_PrefabAccessoryPatterns);
		if (m_iFlashSpeedPacked == 0 && Replication.IsServer()) {
			SetFlashSpeedForStageAuth(1, 2);
			SetFlashSpeedForStageAuth(2, 1);
			SetFlashSpeedForStageAuth(3, 0);
		}
		ClampStage();
		ClampAllPatternIndices();
		ApplyState();
		AddRemoveVehicleDamageListener(true);
		m_Input = GetGame().GetInputManager();
		if (m_Input) GetGame().GetCallqueue().CallLater(RefreshInputBinding, 5000, true);
	}

	// ------------------------------------------------------------
	// Public API
	// ------------------------------------------------------------
	int GetStage() { return m_iStage; }
	int GetLastStage() { return m_iLastStage; }
	bool IsHudHidden() { return m_HideHUD; }

	string GetStageLabel() {
		switch (m_iStage) {
			case 0: return "Stage 0 (Off)";
			case 1: return "Stage 1 (Warning)";
			case 2: return "Stage 2 (Mixed)";
			case 3: return "Stage 3 (Primary)";
		}
		return "Stage (Unknown)";
	}

	string GetPatternLabel(int group) {
		group = ClampGroup(group);
		DD_EmergencyPatternDef p = GetActivePattern(group);
		if (p && !p.Name.IsEmpty()) return p.Name;
		return "Unknown";
	}

	string GetFlashSpeedLabel() {
		int st = GetActiveSpeedStage();
		return SpeedIndexToLabel(GetFlashSpeedIndexForStage(st));
	}

	string GetPauseSpeedLabel() {
		int st = GetActiveSpeedStage();
		return SpeedIndexToLabel(GetPauseSpeedIndexForStage(st));
	}

	string GetFlashSpeedLabelForStage(int stage) {
		return SpeedIndexToLabel(GetFlashSpeedIndexForStage(stage));
	}

	string GetPauseSpeedLabelForStage(int stage) {
		return SpeedIndexToLabel(GetPauseSpeedIndexForStage(stage));
	}
	
	bool HasGroupLights(int groupId) {
		switch (groupId) {
			case DD_ELS_GROUP_PRIMARY:   return m_PrimaryIDs   && m_PrimaryIDs.Count()   > 0;
			case DD_ELS_GROUP_SECONDARY: return m_SecondaryIDs && m_SecondaryIDs.Count() > 0;
			case DD_ELS_GROUP_WARNING:   return m_WarningIDs   && m_WarningIDs.Count()   > 0;
			case DD_ELS_GROUP_ACCESSORY: return m_AccessoryIDs && m_AccessoryIDs.Count() > 0;
		}
		return false;
	}

	void RequestEnableELS(int stage) {
		if (m_bDestroyed) return;
		if (Replication.IsServer()) { EnableELSAuth(stage); return; }
		Rpc(RpcAsk_EnableELS, stage);
	}
	
	void RequestDisableELS() {
		if (m_bDestroyed) return;
		if (Replication.IsServer()) { DisableELSAuth(); return; }
		Rpc(RpcAsk_DisableELS);
	}

	void RequestToggleELS() {
		if (m_bDestroyed) return;
		if (Replication.IsServer()) { ToggleELSAuth(); return; }
		Rpc(RpcAsk_ToggleELS);
	}

	void RequestNextStage() {
		if (m_bDestroyed) return;
		if (Replication.IsServer()) { SetStageAuth(GetNextStage(m_iStage)); return; }
		Rpc(RpcAsk_NextStage);
	}

	void RequestPrevStage() {
		if (m_bDestroyed) return;
		if (Replication.IsServer()) { SetStageAuth(GetPrevStage(m_iStage)); return; }
		Rpc(RpcAsk_PrevStage);
	}

	void RequestNextPattern(int group) {
		if (m_bDestroyed) return;
		group = ClampGroup(group);
		if (Replication.IsServer()) { SetPatternAuth(group, GetNextPatternIndex(group)); return; }
		Rpc(RpcAsk_NextPattern, group);
	}

	void RequestPrevPattern(int group) {
		if (m_bDestroyed) return;
		group = ClampGroup(group);
		if (Replication.IsServer()) { SetPatternAuth(group, GetPrevPatternIndex(group)); return; }
		Rpc(RpcAsk_PrevPattern, group);
	}

	void RequestNextFlashSpeed() {
		if (m_bDestroyed) return;
		int st = GetActiveSpeedStage();
		if (Replication.IsServer()) {
			int cur = GetFlashSpeedIndexForStage(st);
			SetFlashSpeedForStageAuth(st, ClampSpeedIndex(cur + 1));
			return;
		}
		Rpc(RpcAsk_RaiseFlashSpeedForStage, st);
	}

	void RequestPrevFlashSpeed() {
		if (m_bDestroyed) return;
		int st = GetActiveSpeedStage();
		if (Replication.IsServer()) {
			int cur = GetFlashSpeedIndexForStage(st);
			SetFlashSpeedForStageAuth(st, ClampSpeedIndex(cur - 1));
			return;
		}
		Rpc(RpcAsk_LowerFlashSpeedForStage, st);
	}

	void RequestNextPauseSpeed() {
		if (m_bDestroyed) return;
		int st = GetActiveSpeedStage();
		if (Replication.IsServer()) {
			int cur = GetPauseSpeedIndexForStage(st);
			SetPauseSpeedForStageAuth(st, ClampSpeedIndex(cur + 1));
			return;
		}
		Rpc(RpcAsk_RaisePauseSpeedForStage, st);
	}

	void RequestPrevPauseSpeed() {
		if (m_bDestroyed) return;
		int st = GetActiveSpeedStage();
		if (Replication.IsServer()) {
			int cur = GetPauseSpeedIndexForStage(st);
			SetPauseSpeedForStageAuth(st, ClampSpeedIndex(cur - 1));
			return;
		}
		Rpc(RpcAsk_LowerPauseSpeedForStage, st);
	}


	// ------------------------------------------------------------
	// Authority / replication helpers
	// ------------------------------------------------------------
	protected bool IsAuthority() {
		if (!m_Rpl) return false;
		return m_Rpl.Role() == RplRole.Authority;
	}

	protected int ClampGroup(int group) {
		if (group < 0) return 0;
		if (group > 3) return 3;
		return group;
	}

	protected void ClampStage() {
		if (m_iStage < 0) m_iStage = 0;
		if (m_iStage > 3) m_iStage = 3;
	}

	protected void ClampAllPatternIndices() {
		m_iPrimaryPattern   = ClampPatternIndex(m_iPrimaryPattern,   m_PrimaryPatterns);
		m_iSecondaryPattern = ClampPatternIndex(m_iSecondaryPattern, m_SecondaryPatterns);
		m_iWarningPattern   = ClampPatternIndex(m_iWarningPattern,   m_WarningPatterns);
		m_iAccessoryPattern = ClampPatternIndex(m_iAccessoryPattern, m_AccessoryPatterns);
	}

	protected int ClampPatternIndex(int idx, array<ref DD_EmergencyPatternDef> list) {
		int count = list.Count();
		if (count <= 0) return 0;
		if (idx < 0 || idx >= count) return 0;
		return idx;
	}

	protected int ClampSpeedIndex(int idx) {
		if (idx < 0) return 0;
		if (idx > 2) return 2;
		return idx;
	}

	protected int ClampSpeedStage(int stage) {
		if (stage < 1) return 1;
		if (stage > 3) return 3;
		return stage;
	}

	protected void UpdateLastStage(int stage) {
		if (stage >= 1 && stage <= 3) m_iLastStage = stage;
	}

	protected int GetNextStage(int current) {
		int nextUp = current + 1;
		if (nextUp > 3) nextUp = 0;
		return nextUp;
	}

	protected int GetPrevStage(int current) {
		int prev = current - 1;
		if (prev < 0) prev = 3;
		return prev;
	}

	protected int GetNextPatternIndex(int group) {
		group = ClampGroup(group);
		array<ref DD_EmergencyPatternDef> list = GetPatternList(group);
		int count = list.Count();
		if (count <= 0) return 0;
		int cur = GetPatternIndex(group);
		return (cur + 1) % count;
	}

	protected int GetPrevPatternIndex(int group) {
		group = ClampGroup(group);
		array<ref DD_EmergencyPatternDef> list = GetPatternList(group);
		int count = list.Count();
		if (count <= 0) return 0;
		int cur = GetPatternIndex(group);
		int prev = cur - 1;
		if (prev < 0) prev = count - 1;
		return prev;
	}

	protected int GetPatternIndex(int group) {
		switch (group) {
			case DD_ELS_GROUP_PRIMARY:   return m_iPrimaryPattern;
			case DD_ELS_GROUP_SECONDARY: return m_iSecondaryPattern;
			case DD_ELS_GROUP_WARNING:   return m_iWarningPattern;
			case DD_ELS_GROUP_ACCESSORY: return m_iAccessoryPattern;
		}
		return 0;
	}

	protected void SetPatternIndex(int group, int value) {
		switch (group) {
			case DD_ELS_GROUP_PRIMARY:   m_iPrimaryPattern   = value; break;
			case DD_ELS_GROUP_SECONDARY: m_iSecondaryPattern = value; break;
			case DD_ELS_GROUP_WARNING:   m_iWarningPattern   = value; break;
			case DD_ELS_GROUP_ACCESSORY: m_iAccessoryPattern = value; break;
		}
	}

	protected array<ref DD_EmergencyPatternDef> GetPatternList(int group) {
		switch (group) {
			case DD_ELS_GROUP_PRIMARY:   return m_PrimaryPatterns;
			case DD_ELS_GROUP_SECONDARY: return m_SecondaryPatterns;
			case DD_ELS_GROUP_WARNING:   return m_WarningPatterns;
			case DD_ELS_GROUP_ACCESSORY: return m_AccessoryPatterns;
		}
		return m_PrimaryPatterns;
	}

	protected void ResetGroupSequencer(int group) {
		switch (group) {
			case DD_ELS_GROUP_PRIMARY:   m_PrimaryStepIndex = 0;   m_PrimaryStepTimer = 0;   break;
			case DD_ELS_GROUP_SECONDARY: m_SecondaryStepIndex = 0; m_SecondaryStepTimer = 0; break;
			case DD_ELS_GROUP_WARNING:   m_WarningStepIndex = 0;   m_WarningStepTimer = 0;   break;
			case DD_ELS_GROUP_ACCESSORY: m_AccessoryStepIndex = 0; m_AccessoryStepTimer = 0; break;
		}
	}

	// ------------------------------------------------------------
	// Stage-aware speed packing
	// ------------------------------------------------------------
	protected int SetDefaultPackedSpeeds() {
		int packed = 0;
		packed = PackStageValue(packed, 1, 2); // Stage, Speed
		packed = PackStageValue(packed, 2, 1);
		packed = PackStageValue(packed, 3, 0);
		return packed;
	}

	protected int GetActiveSpeedStage() {
		if (m_iStage >= 1 && m_iStage <= 3) return m_iStage;
		return 0;
	}

	protected int PackStageValue(int packed, int stage, int value) {
		stage = ClampSpeedStage(stage);
		value = ClampSpeedIndex(value);
		int shift = (stage - 1) * 2;  // 0,2,4
		int mask = 0x3 << shift;      // two bits
		packed = (packed & ~mask) | ((value & 0x3) << shift);
		return packed;
	}

	protected int UnpackStageValue(int packed, int stage) {
		stage = ClampSpeedStage(stage);
		int shift = (stage - 1) * 2;
		return (packed >> shift) & 0x3;
	}

	protected int GetFlashSpeedIndexForStage(int stage) {
		return ClampSpeedIndex(UnpackStageValue(m_iFlashSpeedPacked, stage));
	}

	protected int GetPauseSpeedIndexForStage(int stage) {
		return ClampSpeedIndex(UnpackStageValue(m_iPauseSpeedPacked, stage));
	}

	protected string SpeedIndexToLabel(int idx) {
		switch (ClampSpeedIndex(idx)) {
			case 0: return "Fast";
			case 1: return "Medium";
			case 2: return "Slow";
		}
		return "Unknown";
	}

	// ------------------------------------------------------------
	// Authority setters (stage, patterns, speeds)
	// ------------------------------------------------------------
	protected void EnableELSAuth(int stage) {
		if (stage < 1 || stage > 3) stage = m_iLastStage;
		if (stage < 1 || stage > 3) stage = 3;
		SetStageAuth(stage);
	}
	
	protected void DisableELSAuth() {
		UpdateLastStage(m_iStage);
		SetStageAuth(0);
	}
	
	protected void ToggleELSAuth() {
		if (m_iStage == 0) {
			int restore = m_iLastStage;
			if (restore < 1 || restore > 3) restore = 3;
			SetStageAuth(restore);
			return;
		}
		UpdateLastStage(m_iStage);
		SetStageAuth(0);
	}

	protected void SetStageAuth(int stage) {
		m_iStage = stage;
		ClampStage();
		UpdateLastStage(m_iStage);
		if (m_iStage == 0) {
			ResetGroupSequencer(DD_ELS_GROUP_PRIMARY);
			ResetGroupSequencer(DD_ELS_GROUP_SECONDARY);
			ResetGroupSequencer(DD_ELS_GROUP_WARNING);
			ResetGroupSequencer(DD_ELS_GROUP_ACCESSORY);
		}
		Replication.BumpMe();
		ApplyState();
	}

	protected void SetPatternAuth(int group, int idx) {
		group = ClampGroup(group);
		array<ref DD_EmergencyPatternDef> list = GetPatternList(group);
		int clamped = ClampPatternIndex(idx, list);
		SetPatternIndex(group, clamped);
		ResetGroupSequencer(group);
		Replication.BumpMe();
		ApplyState();
	}

	protected void SetFlashSpeedForStageAuth(int stage, int idx) {
		stage = ClampSpeedStage(stage);
		idx = ClampSpeedIndex(idx);
		m_iFlashSpeedPacked = PackStageValue(m_iFlashSpeedPacked, stage, idx);
		Replication.BumpMe();
	}

	protected void SetPauseSpeedForStageAuth(int stage, int idx) {
		stage = ClampSpeedStage(stage);
		idx = ClampSpeedIndex(idx);
		m_iPauseSpeedPacked = PackStageValue(m_iPauseSpeedPacked, stage, idx);
		Replication.BumpMe();
	}
	

	// ------------------------------------------------------------
	// Replication callbacks
	// ------------------------------------------------------------
	protected void OnRplStageChanged() {
		ClampStage();
		ApplyState();
	}

	protected void OnRplPrimaryPatternChanged() {
		m_iPrimaryPattern = ClampPatternIndex(m_iPrimaryPattern, m_PrimaryPatterns);
		ResetGroupSequencer(DD_ELS_GROUP_PRIMARY);
		ApplyState();
	}

	protected void OnRplSecondaryPatternChanged() {
		m_iSecondaryPattern = ClampPatternIndex(m_iSecondaryPattern, m_SecondaryPatterns);
		ResetGroupSequencer(DD_ELS_GROUP_SECONDARY);
		ApplyState();
	}

	protected void OnRplWarningPatternChanged() {
		m_iWarningPattern = ClampPatternIndex(m_iWarningPattern, m_WarningPatterns);
		ResetGroupSequencer(DD_ELS_GROUP_WARNING);
		ApplyState();
	}

	protected void OnRplAccessoryPatternChanged() {
		m_iAccessoryPattern = ClampPatternIndex(m_iAccessoryPattern, m_AccessoryPatterns);
		ResetGroupSequencer(DD_ELS_GROUP_ACCESSORY);
		ApplyState();
	}

	protected void OnRplFlashSpeedChanged() {
	}

	protected void OnRplPauseSpeedChanged() {
	}

	// ------------------------------------------------------------
	// RPC: Client -> Server
	// ------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EnableELS(int stage) { EnableELSAuth(stage); }
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_DisableELS() { DisableELSAuth(); }
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_ToggleELS() { ToggleELSAuth(); }

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_NextStage() { SetStageAuth(GetNextStage(m_iStage)); }

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_PrevStage() { SetStageAuth(GetPrevStage(m_iStage)); }

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_NextPattern(int group) {
		group = ClampGroup(group);
		SetPatternAuth(group, GetNextPatternIndex(group));
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_PrevPattern(int group) {
		group = ClampGroup(group);
		SetPatternAuth(group, GetPrevPatternIndex(group));
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RaiseFlashSpeedForStage(int stage) {
		stage = ClampSpeedStage(stage);
		int cur = GetFlashSpeedIndexForStage(stage);
		SetFlashSpeedForStageAuth(stage, ClampSpeedIndex(cur + 1));
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LowerFlashSpeedForStage(int stage) {
		stage = ClampSpeedStage(stage);
		int cur = GetFlashSpeedIndexForStage(stage);
		SetFlashSpeedForStageAuth(stage, ClampSpeedIndex(cur - 1));
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RaisePauseSpeedForStage(int stage) {
		stage = ClampSpeedStage(stage);
		int cur = GetPauseSpeedIndexForStage(stage);
		SetPauseSpeedForStageAuth(stage, ClampSpeedIndex(cur + 1));
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LowerPauseSpeedForStage(int stage) {
		stage = ClampSpeedStage(stage);
		int cur = GetPauseSpeedIndexForStage(stage);
		SetPauseSpeedForStageAuth(stage, ClampSpeedIndex(cur - 1));
	}
	
	// ------------------------------------------------------------
	// Resolving Vehicle Root
	// ------------------------------------------------------------
	protected IEntity ResolveVehicleRoot() {
		IEntity o = GetOwner();
		IEntity last = o;
		while (o) {
			last = o;
			if (o.FindComponent(BaseLightManagerComponent) || o.FindComponent(SCR_WheeledDamageManagerComponent) || o.FindComponent(SCR_VehicleDamageManagerComponent)) return o;
			o = o.GetParent();
		}
		return last;
	}
	

	// ------------------------------------------------------------
	// Ensuring Arrays 
	// ------------------------------------------------------------
	protected void EnsureArrays() {
		if (!m_AllSlots) m_AllSlots = new array<ELS_LightSlot>();
		if (!m_IdToGroup) m_IdToGroup = new map<int, int>();

		if (!m_PrimaryIDs)   m_PrimaryIDs   = new array<int>();
		if (!m_SecondaryIDs) m_SecondaryIDs = new array<int>();
		if (!m_WarningIDs)   m_WarningIDs   = new array<int>();
		if (!m_AccessoryIDs) m_AccessoryIDs = new array<int>();

		if (!m_Stage2PrimaryIDs) m_Stage2PrimaryIDs = new array<int>();

		if (!m_PrimaryPatterns)   m_PrimaryPatterns   = new array<ref DD_EmergencyPatternDef>();
		if (!m_SecondaryPatterns) m_SecondaryPatterns = new array<ref DD_EmergencyPatternDef>();
		if (!m_WarningPatterns)   m_WarningPatterns   = new array<ref DD_EmergencyPatternDef>();
		if (!m_AccessoryPatterns) m_AccessoryPatterns = new array<ref DD_EmergencyPatternDef>();

		if (!m_PrefabPrimaryPatterns)   m_PrefabPrimaryPatterns   = new array<ref DD_EmergencyPatternDef>();
		if (!m_PrefabSecondaryPatterns) m_PrefabSecondaryPatterns = new array<ref DD_EmergencyPatternDef>();
		if (!m_PrefabWarningPatterns)   m_PrefabWarningPatterns   = new array<ref DD_EmergencyPatternDef>();
		if (!m_PrefabAccessoryPatterns) m_PrefabAccessoryPatterns = new array<ref DD_EmergencyPatternDef>();
	}
	
	protected bool EnsureVisualStateReady() {
		if (!m_VehicleRoot) m_VehicleRoot = ResolveVehicleRoot();
		if (!m_LightMgr && m_VehicleRoot) m_LightMgr = BaseLightManagerComponent.Cast(m_VehicleRoot.FindComponent(BaseLightManagerComponent));
		if (!m_AllSlots) m_AllSlots = new array<ELS_LightSlot>();
		if (m_AllSlots.IsEmpty() && m_LightMgr) BuildSlots();
		return m_LightMgr && m_AllSlots && !m_AllSlots.IsEmpty();
	}

	// ------------------------------------------------------------
	// Slot discovery (ELS_LightSlot by ID)
	// ------------------------------------------------------------
	protected void BuildSlots() {
		m_AllSlots.Clear();
		if (!m_LightMgr) return;
		array<BaseLightSlot> all = {};
		m_LightMgr.GetLights(all);
		foreach (BaseLightSlot baseSlot : all) {
			ELS_LightSlot slot = ELS_LightSlot.Cast(baseSlot);
			if (!slot) continue;
			if (!slot.IsLightFunctional()) continue;
			int id = slot.GetELS_ID();
			if (id <= 0) continue;
			m_AllSlots.Insert(slot);
		}
	}

	// ------------------------------------------------------------
	// Pattern loading (per group)
	// ------------------------------------------------------------
	protected void LoadPatternsForGroup(
		int group, ResourceName setRes,
		out array<ref DD_EmergencyPatternDef> outPatterns,
		array<ref DD_EmergencyPatternDef> prefabAdditions
	) {
		outPatterns.Clear();
		DD_EmergencyPatternSet patternSet;
		if (!setRes.IsEmpty()) {
			Resource resSet = Resource.Load(setRes);
			if (resSet && resSet.IsValid()) {
				BaseContainer bcSet = resSet.GetResource().ToBaseContainer();
				patternSet = DD_EmergencyPatternSet.Cast(BaseContainerTools.CreateInstanceFromContainer(bcSet));
			}
		}

		if (patternSet && patternSet.PatternDefs) {
			foreach (ResourceName patResName : patternSet.PatternDefs) {
				if (patResName.IsEmpty()) continue;
				Resource resPat = Resource.Load(patResName);
				if (!resPat || !resPat.IsValid()) continue;
				BaseContainer bcPat = resPat.GetResource().ToBaseContainer();
				DD_EmergencyPatternDef pat = DD_EmergencyPatternDef.Cast(BaseContainerTools.CreateInstanceFromContainer(bcPat));
				if (!pat) continue;
				outPatterns.Insert(pat);
			}
		}

		if (prefabAdditions) {
			foreach (DD_EmergencyPatternDef over : prefabAdditions) {
				if (!over) continue;
				outPatterns.Insert(over);
			}
		}
	}

	protected DD_EmergencyPatternDef GetActivePattern(int group = DD_ELS_GROUP_PRIMARY) {
		array<ref DD_EmergencyPatternDef> list = GetPatternList(group);
		if (list.IsEmpty()) return null;

		int idx = GetPatternIndex(group);
		if (idx < 0 || idx >= list.Count())
			idx = 0;

		return list[idx];
	}

	// ------------------------------------------------------------
	// Tick control
	// ------------------------------------------------------------
	protected void StartTick() {
		if (m_bTicking) return;
		m_bTicking = true;
		int ms = (int)(m_fTick * 1000);
		if (ms < 10) ms = 10;
		GetGame().GetCallqueue().CallLater(Tick, ms, true);
	}

	protected void StopTick() {
		if (!m_bTicking) return;
		m_bTicking = false;
		GetGame().GetCallqueue().Remove(Tick);
	}

	// ------------------------------------------------------------
	// State application (stage + 4 patterns, independent timers)
	// ------------------------------------------------------------
	protected void ApplyState() {
		if (!EnsureVisualStateReady()) return;		
		if (m_iStage == 0 || m_bDestroyed) {
			StopTick();
			ApplyAllOff();
			return;
		}
		if (HasAnyActivePatternSteps()) {
			StartTick();
			ApplyCombinedMasks();
			return;
		}
		StopTick();
		ApplySteadyStage();
	}

	protected bool HasAnyActivePatternSteps() {
		return HasSteps(GetActivePattern(DD_ELS_GROUP_PRIMARY))
			|| HasSteps(GetActivePattern(DD_ELS_GROUP_SECONDARY))
			|| HasSteps(GetActivePattern(DD_ELS_GROUP_WARNING))
			|| HasSteps(GetActivePattern(DD_ELS_GROUP_ACCESSORY));
	}

	protected bool HasSteps(DD_EmergencyPatternDef p) {
		return p && p.Steps && !p.Steps.IsEmpty();
	}

	protected void Tick() {
		if (!m_bTicking) return;
		if (!EnsureVisualStateReady()) return;
		if (m_iStage == 0 || m_bDestroyed) {
			StopTick();
			ApplyAllOff();
			return;
		}
		if (m_VehDmg && m_VehDmg.IsDestroyed()) {
			m_bDestroyed = true;
			StopTick();
			ApplyAllOff();
			if (Replication.IsServer()) SetStageAuth(0);
			return;
		}
		AdvanceGroup(DD_ELS_GROUP_PRIMARY);
		AdvanceGroup(DD_ELS_GROUP_SECONDARY);
		AdvanceGroup(DD_ELS_GROUP_WARNING);
		AdvanceGroup(DD_ELS_GROUP_ACCESSORY);
		ApplyCombinedMasks();
	}

	protected void AdvanceGroup(int group) {
		DD_EmergencyPatternDef p = GetActivePattern(group);
		if (!HasSteps(p)) return;
		int stepIndex;
		float stepTimer;
		GetGroupSequencer(group, stepIndex, stepTimer);
		stepTimer += m_fTick;
		int count = p.Steps.Count();
		if (stepIndex < 0 || stepIndex >= count) stepIndex = 0;
		DD_EmergencyPatternStep step = p.Steps[stepIndex];
		float dur = GetStepDurationSeconds(step);
		if (dur < 0.01) dur = 0.01;
		while (stepTimer >= dur) {
			stepTimer -= dur;
			stepIndex = (stepIndex + 1) % count;
			step = p.Steps[stepIndex];
			dur = GetStepDurationSeconds(step);
			if (dur < 0.01) dur = 0.01;
		}
		SetGroupSequencer(group, stepIndex, stepTimer);
	}

	protected void GetGroupSequencer(int group, out int stepIndex, out float stepTimer) {
		switch (group) {
			case DD_ELS_GROUP_PRIMARY:   stepIndex = m_PrimaryStepIndex;   stepTimer = m_PrimaryStepTimer;   return;
			case DD_ELS_GROUP_SECONDARY: stepIndex = m_SecondaryStepIndex; stepTimer = m_SecondaryStepTimer; return;
			case DD_ELS_GROUP_WARNING:   stepIndex = m_WarningStepIndex;   stepTimer = m_WarningStepTimer;   return;
			case DD_ELS_GROUP_ACCESSORY: stepIndex = m_AccessoryStepIndex; stepTimer = m_AccessoryStepTimer; return;
		}
		stepIndex = 0;
		stepTimer = 0;
	}

	protected void SetGroupSequencer(int group, int stepIndex, float stepTimer) {
		switch (group) {
			case DD_ELS_GROUP_PRIMARY:   m_PrimaryStepIndex = stepIndex;   m_PrimaryStepTimer = stepTimer;   return;
			case DD_ELS_GROUP_SECONDARY: m_SecondaryStepIndex = stepIndex; m_SecondaryStepTimer = stepTimer; return;
			case DD_ELS_GROUP_WARNING:   m_WarningStepIndex = stepIndex;   m_WarningStepTimer = stepTimer;   return;
			case DD_ELS_GROUP_ACCESSORY: m_AccessoryStepIndex = stepIndex; m_AccessoryStepTimer = stepTimer; return;
		}
	}

	protected int GetGroupCurrentMask(int group) {
		group = ClampGroup(group);
		DD_EmergencyPatternDef p = GetActivePattern(group);
		if (!HasSteps(p)) return -1;
		int stepIndex;
		float stepTimer;
		GetGroupSequencer(group, stepIndex, stepTimer);
		if (stepIndex < 0 || stepIndex >= p.Steps.Count()) stepIndex = 0;
		return p.Steps[stepIndex].Mask;
	}

	protected void ApplyCombinedMasks() {
		int maskPrimary   = GetGroupCurrentMask(DD_ELS_GROUP_PRIMARY);
		int maskSecondary = GetGroupCurrentMask(DD_ELS_GROUP_SECONDARY);
		int maskWarning   = GetGroupCurrentMask(DD_ELS_GROUP_WARNING);
		int maskAccessory = GetGroupCurrentMask(DD_ELS_GROUP_ACCESSORY);
		foreach (ELS_LightSlot slot : m_AllSlots) {
			int id = slot.GetELS_ID();
			bool enabled = ComputeEnabledForSlot(id, maskPrimary, maskSecondary, maskWarning, maskAccessory);
			SetSlotEnabled(slot, enabled);
		}
	}

	protected bool ComputeEnabledForSlot(int id, int maskPrimary, int maskSecondary, int maskWarning, int maskAccessory) {
		if (m_iStage == 0 || !IsIdEnabledForStage(id, m_iStage)) return false; 
		int group = GetGroupForId(id);
		int mask = -1;
		switch (group) {
			case DD_ELS_GROUP_PRIMARY:   mask = maskPrimary; break;
			case DD_ELS_GROUP_SECONDARY: mask = maskSecondary; break;
			case DD_ELS_GROUP_WARNING:   mask = maskWarning; break;
			case DD_ELS_GROUP_ACCESSORY: mask = maskAccessory; break;
		}
		if (mask < 0) return false;
		int bit = id - 1; // ID1 -> bit0
		if (bit < 0 || bit > 23) return false;
		return (((mask >> bit) & 1) == 1);
	}

	protected void ApplySteadyStage() {
		foreach (ELS_LightSlot slot : m_AllSlots) {
			int id = slot.GetELS_ID();
			SetSlotEnabled(slot, IsIdEnabledForStage(id, m_iStage));
		}
	}

	protected void ApplyAllOff() {
		foreach (ELS_LightSlot slot : m_AllSlots)
			SetSlotEnabled(slot, false);
	}

	// ------------------------------------------------------------
	// Dynamic speed per Stage 
	// ------------------------------------------------------------
	protected float SpeedIndexToSeconds(int idx) {
		if (idx <= 0) return 0.10; // 100ms
		if (idx == 1) return 0.25; // 250ms
		return 0.50;               // 500ms
	}

	protected bool IsPauseStep(DD_EmergencyPatternStep step) {
		if (!step) return true;
		if (DD_EmergencyPatternPause.Cast(step)) return true;
		return step.Mask == 0;
	}

	protected float GetStepDurationSeconds(DD_EmergencyPatternStep step) {
		int st = GetActiveSpeedStage();
		int idx = GetFlashSpeedIndexForStage(st);
		if (IsPauseStep(step)) idx = GetPauseSpeedIndexForStage(st);
		float base = SpeedIndexToSeconds(idx);
		float mul = step.Duration;
		if (mul <= 0.0) mul = 1.0;
		return base * mul;
	}

	// ------------------------------------------------------------
	// ID -> Group + Stage rules
	// ------------------------------------------------------------
	protected void BuildIdGroupMap() {
		if (!m_IdToGroup) m_IdToGroup = new map<int, int>(); else m_IdToGroup.Clear();
		InsertGroupIds(m_PrimaryIDs,   DD_ELS_GROUP_PRIMARY);
		InsertGroupIds(m_SecondaryIDs, DD_ELS_GROUP_SECONDARY);
		InsertGroupIds(m_WarningIDs,   DD_ELS_GROUP_WARNING);
		InsertGroupIds(m_AccessoryIDs, DD_ELS_GROUP_ACCESSORY);
	}

	protected void InsertGroupIds(array<int> ids, int group) {
		if (!ids) return;
		foreach (int id : ids) {
			if (id <= 0) continue;
			if (m_IdToGroup.Contains(id)) Print(string.Format("[ELS] ID %1 assigned to multiple groups on %2", id, m_VehicleRoot), LogLevel.WARNING);
			m_IdToGroup.Set(id, group); // last one wins if multiple
		}
	}

	protected int GetGroupForId(int id) {
		if (m_IdToGroup) {
			int g;
			if (m_IdToGroup.Find(id, g)) return g;
		}
		return -1;
	}

	protected bool IsIdEnabledForStage(int id, int stage) {
		int group = GetGroupForId(id);
		if (group < 0) return false;
		switch (stage) {
			case 1: {
				if (group == DD_ELS_GROUP_WARNING)   return m_bStage1Warning;
				if (group == DD_ELS_GROUP_ACCESSORY) return m_bStage1Accessory;
				return false;
			}
			case 2: {
				if (group == DD_ELS_GROUP_WARNING)   return m_bStage2Warning;
				if (group == DD_ELS_GROUP_SECONDARY) return m_bStage2Secondary;
				if (group == DD_ELS_GROUP_ACCESSORY) return m_bStage2Accessory;

				if (group == DD_ELS_GROUP_PRIMARY)
				{
					if (!m_bStage2PrimaryPartial) return true;
					if (!m_Stage2PrimaryIDs) return false;

					foreach (int pid : m_Stage2PrimaryIDs)
						if (pid == id) return true;

					return false;
				}

				return false;
			}
			case 3: {
				if (group == DD_ELS_GROUP_PRIMARY)   return m_bStage3Primary;
				if (group == DD_ELS_GROUP_SECONDARY) return m_bStage3Secondary;
				if (group == DD_ELS_GROUP_ACCESSORY) return m_bStage3Accessory;
				if (group == DD_ELS_GROUP_WARNING)   return m_bStage3Warning;
				return false;
			}
		}
		return false;
	}
	

	// ------------------------------------------------------------
	// LightEntity driving
	// ------------------------------------------------------------
	protected void SetSlotEnabled(ELS_LightSlot slot, bool enabled) {
		if (!slot) return;
		LightEntity light = LightEntity.Cast(slot.GetLightEntity());
		if (light) light.SetEnabled(enabled);
	}

	// ------------------------------------------------------------
	// Check Local Player
	// ------------------------------------------------------------
	protected bool IsLocalPlayerDrivingThisVehicle() {
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc) return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(pc.GetControlledEntity());
		if (!character) return false;
		IEntity vehRoot = CompartmentAccessComponent.GetVehicleIn(character);
		if (!vehRoot) return false;
		if (vehRoot != GetOwner()) return false;
		Vehicle v = Vehicle.Cast(vehRoot);
		if (v) return v.GetPilot() == character;
		return true;
	}
	
	// ------------------------------------------------------------
	// Input Listening
	// ------------------------------------------------------------
	protected void RefreshInputBinding() {
		if (!m_Input) m_Input = GetGame().GetInputManager();
		if (!m_Input) return;
		bool shouldBind = IsLocalPlayerDrivingThisVehicle();
		if (!shouldBind) {
			if (m_Bound) UnbindInput();
			return;
		}
		bool ok = m_Input.ActivateContext("ELS_Context", 5000);
		if (!ok) return;
		if (!m_Bound) BindInput();
	}

	protected void BindInput() {
		m_Bound = true;
		m_Input.AddActionListener("DD_ELS_ToggleLights", EActionTrigger.DOWN, OnELS_Toggle);
		m_Input.AddActionListener("DD_ELS_StageUp", EActionTrigger.DOWN, OnELS_StageUp);
		m_Input.AddActionListener("DD_ELS_StageDown", EActionTrigger.DOWN, OnELS_StageDown);
		m_Input.AddActionListener("DD_ELS_FlashFaster", EActionTrigger.DOWN, OnELS_FlashFaster);
		m_Input.AddActionListener("DD_ELS_FlashSlower", EActionTrigger.DOWN, OnELS_FlashSlower);
		m_Input.AddActionListener("DD_ELS_PauseShorter", EActionTrigger.DOWN, OnELS_PauseShorter);
		m_Input.AddActionListener("DD_ELS_PauseLonger", EActionTrigger.DOWN, OnELS_PauseLonger);
		m_Input.AddActionListener("DD_ELS_PrimaryNext", EActionTrigger.DOWN, OnELS_PrimaryCycleNext);
		m_Input.AddActionListener("DD_ELS_SecondaryNext", EActionTrigger.DOWN, OnELS_SecondaryCycleNext);
		m_Input.AddActionListener("DD_ELS_WarningNext", EActionTrigger.DOWN, OnELS_WarningCycleNext);
		m_Input.AddActionListener("DD_ELS_AccessoryNext", EActionTrigger.DOWN, OnELS_AccessoryCycleNext);
		m_Input.AddActionListener("DD_ELS_PrimaryPrev", EActionTrigger.DOWN, OnELS_PrimaryCyclePrev);
		m_Input.AddActionListener("DD_ELS_SecondaryPrev", EActionTrigger.DOWN, OnELS_SecondaryCyclePrev);
		m_Input.AddActionListener("DD_ELS_WarningPrev", EActionTrigger.DOWN, OnELS_WarningCyclePrev);
		m_Input.AddActionListener("DD_ELS_AccessoryPrev", EActionTrigger.DOWN, OnELS_AccessoryCyclePrev);
		m_Input.AddActionListener("DD_ELS_ToggleHUD", EActionTrigger.DOWN, OnToggleHud);
	}

	protected void UnbindInput() {
		m_Bound = false;
		m_Input.RemoveActionListener("DD_ELS_ToggleLights", EActionTrigger.DOWN, OnELS_Toggle);
		m_Input.RemoveActionListener("DD_ELS_StageUp", EActionTrigger.DOWN, OnELS_StageUp);
		m_Input.RemoveActionListener("DD_ELS_StageDown", EActionTrigger.DOWN, OnELS_StageDown);
		m_Input.RemoveActionListener("DD_ELS_FlashFaster", EActionTrigger.DOWN, OnELS_FlashFaster);
		m_Input.RemoveActionListener("DD_ELS_FlashSlower", EActionTrigger.DOWN, OnELS_FlashSlower);
		m_Input.RemoveActionListener("DD_ELS_PauseShorter", EActionTrigger.DOWN, OnELS_PauseShorter);
		m_Input.RemoveActionListener("DD_ELS_PauseLonger", EActionTrigger.DOWN, OnELS_PauseLonger);
		m_Input.RemoveActionListener("DD_ELS_PrimaryNext", EActionTrigger.DOWN, OnELS_PrimaryCycleNext);
		m_Input.RemoveActionListener("DD_ELS_SecondaryNext", EActionTrigger.DOWN, OnELS_SecondaryCycleNext);
		m_Input.RemoveActionListener("DD_ELS_WarningNext", EActionTrigger.DOWN, OnELS_WarningCycleNext);
		m_Input.RemoveActionListener("DD_ELS_AccessoryNext", EActionTrigger.DOWN, OnELS_AccessoryCycleNext);
		m_Input.RemoveActionListener("DD_ELS_PrimaryPrev", EActionTrigger.DOWN, OnELS_PrimaryCyclePrev);
		m_Input.RemoveActionListener("DD_ELS_SecondaryPrev", EActionTrigger.DOWN, OnELS_SecondaryCyclePrev);
		m_Input.RemoveActionListener("DD_ELS_WarningPrev", EActionTrigger.DOWN, OnELS_WarningCyclePrev);
		m_Input.RemoveActionListener("DD_ELS_AccessoryPrev", EActionTrigger.DOWN, OnELS_AccessoryCyclePrev);
		m_Input.RemoveActionListener("DD_ELS_ToggleHUD", EActionTrigger.DOWN, OnToggleHud);
	}
	
	protected void ELS_ActionNotifyAndSound(string notification) {
		if (m_VehSound) m_VehSound.SoundEvent(SCR_SoundEvent.SOUND_VEHICLE_CLOSE_LIGHT_ON);
		if (!m_HideHUD) SCR_HintManagerComponent.ShowCustomHint(notification, "Emergency Lights", 2.0, true);
	}
	
	protected void OnELS_Toggle(float value, EActionTrigger reason) {
		if (m_VehSound) m_VehSound.SoundEvent(SCR_SoundEvent.SOUND_VEHICLE_CLOSE_LIGHT_ON);
		this.RequestToggleELS();
	}
	
	protected void OnELS_StageUp(float value, EActionTrigger reason) {
		RequestNextStage();
		ELS_ActionNotifyAndSound(string.Format("ELS Stage: %1", GetStageLabel()));
	}
	
	protected void OnELS_StageDown(float value, EActionTrigger reason) {
		RequestPrevStage();
		ELS_ActionNotifyAndSound(string.Format("ELS Stage: %1", GetStageLabel()));
	}
	
	protected void OnELS_FlashFaster(float value, EActionTrigger reason) {
		RequestPrevFlashSpeed();
		ELS_ActionNotifyAndSound(string.Format("ELS Flash Speed: %1", GetFlashSpeedLabel()));
	}
	
	protected void OnELS_FlashSlower(float value, EActionTrigger reason) {
		RequestNextFlashSpeed();
		ELS_ActionNotifyAndSound(string.Format("ELS Flash Speed: %1", GetFlashSpeedLabel()));
	}
	
	protected void OnELS_PauseShorter(float value, EActionTrigger reason) {
		RequestNextPauseSpeed();
		ELS_ActionNotifyAndSound(string.Format("ELS Pause Length: %1", GetPauseSpeedLabel()));
	}
	
	protected void OnELS_PauseLonger(float value, EActionTrigger reason) {
		RequestPrevPauseSpeed();
		ELS_ActionNotifyAndSound(string.Format("ELS Pause Length: %1", GetPauseSpeedLabel()));
	}
	
	protected void OnELS_PrimaryCycleNext(float value, EActionTrigger reason) {
		RequestNextPattern(0);
		ELS_ActionNotifyAndSound(string.Format("ELS Pattern: %1", GetPatternLabel(0)));
	}
	
	protected void OnELS_SecondaryCycleNext(float value, EActionTrigger reason) {
		RequestNextPattern(1);
		ELS_ActionNotifyAndSound(string.Format("ELS Pattern: %1", GetPatternLabel(1)));
	}
	
	protected void OnELS_WarningCycleNext(float value, EActionTrigger reason) {
		RequestNextPattern(2);
		ELS_ActionNotifyAndSound(string.Format("ELS Pattern: %1", GetPatternLabel(2)));
	}
	
	protected void OnELS_AccessoryCycleNext(float value, EActionTrigger reason) {
		RequestNextPattern(3);
		ELS_ActionNotifyAndSound(string.Format("ELS Pattern: %1", GetPatternLabel(3)));
	}
	
	protected void OnELS_PrimaryCyclePrev(float value, EActionTrigger reason) {
		RequestPrevPattern(0);
		ELS_ActionNotifyAndSound(string.Format("ELS Pattern: %1", GetPatternLabel(0)));
	}
	
	protected void OnELS_SecondaryCyclePrev(float value, EActionTrigger reason) {
		RequestPrevPattern(1);
		ELS_ActionNotifyAndSound(string.Format("ELS Pattern: %1", GetPatternLabel(1)));
	}
	
	protected void OnELS_WarningCyclePrev(float value, EActionTrigger reason) {
		RequestPrevPattern(2);
		ELS_ActionNotifyAndSound(string.Format("ELS Pattern: %1", GetPatternLabel(2)));
	}
	
	protected void OnELS_AccessoryCyclePrev(float value, EActionTrigger reason) {
		RequestPrevPattern(3);
		ELS_ActionNotifyAndSound(string.Format("ELS Pattern: %1", GetPatternLabel(3)));
	}
	
	protected void OnToggleHud(float value, EActionTrigger reason) {
		m_HideHUD = !m_HideHUD;
		if (m_HideHUD) SCR_HintManagerComponent.ShowCustomHint("ELS HUD Hidden", "Emergency Lights", 2.0, true);
	}
	
	// ------------------------------------------------------------
	// Destroy handling
	// ------------------------------------------------------------
	protected void AddRemoveVehicleDamageListener(bool add) {
		if (!m_VehicleRoot) m_VehicleRoot = ResolveVehicleRoot();
		if (!m_VehicleRoot) return;
		m_VehDmg = SCR_VehicleDamageManagerComponent.Cast(m_VehicleRoot.FindComponent(SCR_VehicleDamageManagerComponent));
		if (!m_VehDmg) return;
		if (add)
			m_VehDmg.GetOnDamageStateChanged().Insert(OnDamageStateChanged);
		else
			m_VehDmg.GetOnDamageStateChanged().Remove(OnDamageStateChanged);
	}

	protected void OnDamageStateChanged(EDamageState newState, EDamageState prevState, bool isJIP) {
		if (!m_VehDmg) return;
		if (newState == EDamageState.DESTROYED || m_VehDmg.IsDestroyed()) {
			m_bDestroyed = true;
			StopTick();
			ApplyAllOff();
			if (Replication.IsServer()) SetStageAuth(0);
		}
	}

	override void OnDelete(IEntity owner) {
		super.OnDelete(owner);
		StopTick();
		ApplyAllOff();
		AddRemoveVehicleDamageListener(false);
		if (m_Bound) UnbindInput();
	}
	
	bool HasLocalEffectOnlyScript() { return false; }
}

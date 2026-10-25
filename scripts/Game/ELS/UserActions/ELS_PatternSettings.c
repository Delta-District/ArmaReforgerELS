
modded class ELS_EmergencyLightController
{
	static const float DD_ELS_SETTINGS_TIMEOUT_S = 15000.0;
	static const int DD_ELS_MENU_FLASH   = 0;
	static const int DD_ELS_MENU_PATTERN = 1;
	protected ref map<int, bool>  m_mFlashOpen;
	protected ref map<int, float> m_mFlashExpiry;
	protected ref map<int, bool>  m_mPatternOpen;
	protected ref map<int, float> m_mPatternExpiry;
	protected bool m_bSettingsTimeoutTicking;

	protected void EnsureSettingsMaps() {
		if (!m_mFlashOpen)      m_mFlashOpen = new map<int, bool>();
		if (!m_mFlashExpiry)    m_mFlashExpiry = new map<int, float>();
		if (!m_mPatternOpen)    m_mPatternOpen = new map<int, bool>();
		if (!m_mPatternExpiry)  m_mPatternExpiry = new map<int, float>();
	}

	protected int GetPlayerKey(IEntity userEntity) {
		if (!userEntity) return -1;
		return userEntity.GetID();
	}

	protected float NowS() {
		World w = GetGame().GetWorld();
		if (!w) return 0.0;
		return w.GetWorldTime();
	}

	bool IsFlashMenuOpenFor(IEntity userEntity) {
		EnsureSettingsMaps();
		int key = GetPlayerKey(userEntity);
		if (key < 0) return false;
		bool open;
		if (m_mFlashOpen.Find(key, open)) return open;
		return false;
	}

	bool IsPatternMenuOpenFor(IEntity userEntity) {
		EnsureSettingsMaps();
		int key = GetPlayerKey(userEntity);
		if (key < 0) return false;
		bool open;
		if (m_mPatternOpen.Find(key, open)) return open;
		return false;
	}

	void ToggleFlashMenuFor(IEntity userEntity) {
		EnsureSettingsMaps();
		int key = GetPlayerKey(userEntity);
		if (key < 0) return;
		bool open = IsFlashMenuOpenFor(userEntity);
		if (!open) ClosePatternMenuFor(userEntity);
		if (open) {
			m_mFlashOpen.Set(key, false);
			m_mFlashExpiry.Remove(key);
		} else {
			m_mFlashOpen.Set(key, true);
			m_mFlashExpiry.Set(key, NowS() + DD_ELS_SETTINGS_TIMEOUT_S);
			StartSettingsTimeoutTick();
		}
		StopSettingsTimeoutTickIfIdle();
	}

	void TogglePatternMenuFor(IEntity userEntity) {
		EnsureSettingsMaps();
		int key = GetPlayerKey(userEntity);
		if (key < 0) return;
		bool open = IsPatternMenuOpenFor(userEntity);
		if (!open) CloseFlashMenuFor(userEntity);
		if (open) {
			m_mPatternOpen.Set(key, false);
			m_mPatternExpiry.Remove(key);
		} else {
			m_mPatternOpen.Set(key, true);
			m_mPatternExpiry.Set(key, NowS() + DD_ELS_SETTINGS_TIMEOUT_S);
			StartSettingsTimeoutTick();
		}
		StopSettingsTimeoutTickIfIdle();
	}

	void TouchFlashMenuFor(IEntity userEntity) {
		EnsureSettingsMaps();
		int key = GetPlayerKey(userEntity);
		if (key < 0) return;
		m_mFlashOpen.Set(key, true);
		m_mFlashExpiry.Set(key, NowS() + DD_ELS_SETTINGS_TIMEOUT_S);
		StartSettingsTimeoutTick();
	}

	void TouchPatternMenuFor(IEntity userEntity) {
		EnsureSettingsMaps();
		int key = GetPlayerKey(userEntity);
		if (key < 0) return;
		m_mPatternOpen.Set(key, true);
		m_mPatternExpiry.Set(key, NowS() + DD_ELS_SETTINGS_TIMEOUT_S);
		StartSettingsTimeoutTick();
	}

	void CloseFlashMenuFor(IEntity userEntity) {
		EnsureSettingsMaps();
		int key = GetPlayerKey(userEntity);
		if (key < 0) return;
		m_mFlashOpen.Set(key, false);
		m_mFlashExpiry.Remove(key);
		StopSettingsTimeoutTickIfIdle();
	}

	void ClosePatternMenuFor(IEntity userEntity) {
		EnsureSettingsMaps();
		int key = GetPlayerKey(userEntity);
		if (key < 0) return;
		m_mPatternOpen.Set(key, false);
		m_mPatternExpiry.Remove(key);
		StopSettingsTimeoutTickIfIdle();
	}

	protected void StartSettingsTimeoutTick() {
		if (m_bSettingsTimeoutTicking) return;
		m_bSettingsTimeoutTicking = true;
		GetGame().GetCallqueue().CallLater(CheckSettingsTimeouts, 1000, true);
	}

	protected void StopSettingsTimeoutTickIfIdle() {
		if (!m_bSettingsTimeoutTicking) return;

		foreach (int k1, bool o1 : m_mFlashOpen)    if (o1) return;
		foreach (int k2, bool o2 : m_mPatternOpen)  if (o2) return;

		m_bSettingsTimeoutTicking = false;
		GetGame().GetCallqueue().Remove(CheckSettingsTimeouts);
	}

	protected void CheckSettingsTimeouts() {
		if (!m_bSettingsTimeoutTicking) return;
		float now = NowS();
		ref array<int> closeFlash = new array<int>();
		ref array<int> closePat   = new array<int>();
		foreach (int keyF, float expF : m_mFlashExpiry)
			if (now >= expF) closeFlash.Insert(keyF);
		foreach (int keyP, float expP : m_mPatternExpiry)
			if (now >= expP) closePat.Insert(keyP);
		foreach (int kf : closeFlash) {
			m_mFlashOpen.Set(kf, false);
			m_mFlashExpiry.Remove(kf);
		}
		foreach (int kp : closePat) {
			m_mPatternOpen.Set(kp, false);
			m_mPatternExpiry.Remove(kp);
		}
		StopSettingsTimeoutTickIfIdle();
	}
}





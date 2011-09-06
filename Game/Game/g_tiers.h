typedef struct{
	qboolean exists;
	qboolean permanent;
	qboolean requirementButtonUp;
	qboolean requirementButtonDown;
	int capabilities;
	int requirementUseSkill;
	int requirementCurrent;
	int requirementCurrentPercent;
	int requirementMaximum;
	int requirementFatigue;
	int requirementHealth;
	int requirementHealthMaximum;
	int sustainCurrent;
	int sustainCurrentPercent;
	int sustainMaximum;
	int sustainHealth;
	int sustainFatigue;
	int speed;
	int defenseRecoveryDelay;
	int transformFirstDuration;
	int transformFirstFatigue;
	int transformFirstHealth;
	int transformFirstEffectMaximum;
	int transformDuration;
	int transformFatigue;
	int transformHealth;
	int transformEffectMaximum;
	int collisionSize[3];
	int meleeDistance;
	float transformSubsequentDuration;
	float transformSubsequentFatigueScale;
	float transformSubsequentHealthScale;
	float transformSubsequentMaximumScale;
	float defenseMelee;
	float defenseEnergy;
	float defenseCapacity;
	float defenseRecovery;
	float fatigueRecovery;
	float boostCost;
	float zanzokenCost;
	float zanzokenSpeed;
	float zanzokenDistance;
	float airBrakeCost;
	float knockbackPower;
	float knockbackIntensity;
	float meleeAttack;
	float energyAttackDamage;
	float energyAttackCost;
	float transformTime;
	float effectCurrent;
	float effectFatigue;
	float effectMaximum;
	float effectHealth;
	float breakLimitRate;
}tierConfig_g;

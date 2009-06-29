#include "g_local.h"
void parseTier(char *path,tierConfig_g *tier);
void syncTier(gclient_t *client){
	tierConfig_g *tier;
	playerState_t *ps;
	ps = &client->ps;
	tier = &client->tiers[ps->powerLevel[tierCurrent]];
	ps->breakLimitRate = (float)tier->breakLimitRate * g_breakLimitRate.value;
	ps->stats[speed] = tier->speed * 450.0;
	ps->stats[zanzokenDistance] = tier->zanzokenDistance * 500.0;
	ps->stats[zanzokenSpeed] = tier->zanzokenSpeed * 4000.0;
	ps->stats[zanzokenCost] = tier->zanzokenCost;
	ps->stats[airBrakeCost] = tier->airBrakeCost;
	ps->stats[meleeDefense] = tier->meleeDefense;
	ps->stats[meleeAttack] = tier->meleeAttack;
	ps->stats[energyDefense] = tier->energyDefense;
	ps->stats[energyAttack] = tier->energyAttackDamage;
	ps->stats[energyCost] = tier->energyAttackCost;
	ps->powerLevel[drainCurrent] = tier->effectCurrent;
	ps->powerLevel[drainFatigue] = tier->effectFatigue;
	ps->powerLevel[drainHealth] = tier->effectHealth;
	ps->powerLevel[drainMaximum] = tier->effectMaximum;
}
void checkTier(gclient_t *client){
	int tier;
	playerState_t *ps;
	tierConfig_g *nextTier,*baseTier;
	ps = &client->ps;
	if(ps->timers[transform]){return;}
	while(1){
		tier = ps->powerLevel[tierCurrent];
		if(((tier+1) < 8) && (client->tiers[tier+1].exists)){
			nextTier = &client->tiers[tier+1];
			if(((nextTier->requirementButton && (ps->bitFlags & keyTierUp)) || !nextTier->requirementButton) &&
			   (ps->powerLevel[current] >= nextTier->requirementCurrent) &&
			   (ps->powerLevel[fatigue] >= nextTier->requirementFatigue) &&
			   (ps->powerLevel[health] >= nextTier->requirementHealth) &&
			   (ps->powerLevel[maximum] >= nextTier->requirementMaximum)){
				ps->timers[transform] = 1;
				++ps->powerLevel[tierCurrent];
				if(tier + 1 > ps->powerLevel[tierTotal]){
					ps->powerLevel[tierTotal] = ps->powerLevel[tierCurrent];
					ps->timers[transform] = client->tiers[tier+1].transformTime;
				}
				continue;
			}
		}
		if(tier > 0){
			baseTier = &client->tiers[tier];
			if(((baseTier->requirementButton && (ps->bitFlags & keyTierDown)) || !baseTier->requirementButton) ||
			   (ps->powerLevel[current] < baseTier->sustainCurrent) ||
			   (ps->powerLevel[health] < baseTier->sustainHealth) ||
			   (ps->powerLevel[fatigue] < baseTier->sustainFatigue) ||
			   (ps->powerLevel[maximum] < baseTier->sustainMaximum)){
				ps->timers[transform] = -1;
				--ps->powerLevel[tierCurrent];
				continue;
			}
		}
		break;
	}
}
void setupTiers(gclient_t *client){
	int	i;
	tierConfig_g *tier;
	char *modelName;
	char *tierPath;
	char tempPath[MAX_QPATH];
	modelName = client->modelName;
	for(i=0;i<8;i++){
		tier = &client->tiers[i];
		memset(tier,0,sizeof(tierConfig_g));
		tierPath = va("players/%s/tier%i/",modelName,i+1);
		parseTier("players/tierDefault.cfg",tier);
		parseTier(strcat(tierPath,"tier.cfg"),tier);
	}
	syncTier(client);
}
void parseTier(char *path,tierConfig_g *tier){
	fileHandle_t tierCFG;
	int i;
	char *token,*parse;
	int fileLength;
	char fileContents[32000];
	tier->exists = qfalse;
	if(trap_FS_FOpenFile(path,0,FS_READ)>0){
		tier->exists = qtrue;
		fileLength = trap_FS_FOpenFile(path,&tierCFG,FS_READ);
		trap_FS_Read(fileContents,fileLength,tierCFG);
		fileContents[fileLength] = 0;
		trap_FS_FCloseFile(tierCFG);
		parse = fileContents;
		while(1){
			token = COM_Parse(&parse);
			if(!token[0]){break;}
			else if(!Q_stricmp(token,"tierSpeed")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->speed = atof(token);
			}
			else if(!Q_stricmp(token,"percentMeleeAttack")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->meleeAttack = atof(token);
			}
			else if(!Q_stricmp(token,"percentMeleeDefense")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->meleeDefense = atoi(token);
			}
			else if(!Q_stricmp(token,"percentEnergyDefense")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->energyDefense = atoi(token);
			}
			else if(!Q_stricmp(token,"percentEnergyAttackDamage")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->energyAttackDamage = atof(token);
			}
			else if(!Q_stricmp(token,"percentEnergyAttackCost")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->energyAttackCost = atof(token);
			}
			else if(!Q_stricmp(token,"tierKnockBackPower")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->knockBackPower = atof(token);
			}
			else if(!Q_stricmp(token,"tierAirBrakeCost")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->airBrakeCost = atof(token);
			}
			else if(!Q_stricmp(token,"tierZanzokenCost")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->zanzokenCost = atof(token);
			}
			else if(!Q_stricmp(token,"tierZanzokenSpeed")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->zanzokenSpeed = atof(token);
			}
			else if(!Q_stricmp(token,"tierZanzokenDistance")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->zanzokenDistance = atof(token);
			}
			else if(!Q_stricmp(token,"breakLimitRate")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->breakLimitRate = atof(token);
			}
			else if(!Q_stricmp(token,"effectCurrent")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->effectCurrent = atoi(token);
			}
			else if(!Q_stricmp(token,"effectMaximum")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->effectMaximum = atoi(token);
			}
			else if(!Q_stricmp(token,"effectFatigue")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->effectFatigue = atoi(token);
			}
			else if(!Q_stricmp(token,"effectHealth")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->effectHealth = atoi(token);
			}
			else if(!Q_stricmp(token,"requirementCurrent")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->requirementCurrent = atoi(token);
			}
			else if(!Q_stricmp(token,"requirementMaximum")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->requirementMaximum = atoi(token);
			}
			else if(!Q_stricmp(token,"requirementHealth")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->requirementHealth = atoi(token);
			}
			else if(!Q_stricmp(token,"requirementFatigue")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->requirementFatigue = atoi(token);
			}
			else if(!Q_stricmp(token,"sustainCurrent")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->sustainCurrent = atoi(token);
			}
			else if(!Q_stricmp(token,"sustainHealth")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->sustainHealth = atoi(token);
			}
			else if(!Q_stricmp(token,"sustainFatigue")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->sustainFatigue = atoi(token);
			}
			else if(!Q_stricmp(token,"sustainMaximum")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->sustainMaximum = atoi(token);
			}
			else if(!Q_stricmp(token,"transformTime")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->transformTime = atoi(token);
			}
			else if(!Q_stricmp(token,"requirementButton")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->requirementButton = strlen(token) == 4 ? qtrue : qfalse;
			}
			else if(!Q_stricmp(token,"tierPermanent")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->permanent = strlen(token) == 4 ? qtrue : qfalse;
			}
			else if(!Q_stricmp(token,"tierCustomWeapons")){
				token = COM_Parse(&parse);
				if(!token[0]){break;}
				tier->customWeapons = strlen(token) == 4 ? qtrue : qfalse;
			}
		}
	}
}

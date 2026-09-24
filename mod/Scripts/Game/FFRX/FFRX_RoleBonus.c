// FF - REMIXED - PVE
// BONUS DE ROLE : ce que sait faire une escouade specialisee que les autres ne savent pas.
//
// Deux roles couverts, marques par un drapeau dans ffrx-groups.json :
//   "genie" -> construit plus vite            (partie GENIE, ci-dessous)
//   "medic" -> soigne mieux, gaspille moins   (partie SANTE, en bas de fichier)
//
// Aujourd'hui ECHO porte le drapeau genie et JULIETT le drapeau medic. C'est une donnee
// de CONFIG et non une constante du code : renommer ou reorganiser les escouades depuis
// le site ne casse rien, et un serveur peut en avoir deux, ou aucune.
//
// ====================================================================================
// GENIE -- CE QUE CA FAIT
//
// Un joueur membre d'une escouade marquee "genie" dans ffrx-groups.json depose plus de
// valeur de construction a chaque coup de pelle. Le chantier monte donc plus vite, mais
// il coute exactement le meme ravitaillement : on achete du TEMPS, pas une remise.
//
// C'est le premier vrai metier du jeu -- jusqu'ici une escouade du genie n'etait qu'un
// nom sur la carte. Maintenant, emmener un sapeur sur une position change la donne.
//
// ------------------------------------------------------------------------------------
// COMMENT MARCHE LA CONSTRUCTION DANS LE JEU DE BASE (releve dans les sources)
//
//   SCR_CampaignBuildingBuildUserAction.PerformAction   (CLIENT, action repetee)
//     -> GetBuildingToolValue(user)  = m_iConstructionValue de l'outil tenu
//        (notre ETool_ALICE porte 10, qui est aussi le defaut du composant)
//     -> SCR_CampaignBuildingNetworkComponent.AddBuildingValue(valeur, chantier)
//          -> Rpc(RpcAsk_AddBuildingValue, ...)          [CLIENT -> SERVEUR]
//               -> SCR_CampaignBuildingLayoutComponent.AddBuildingValue(valeur)
//                    m_fCurrentBuildValue += valeur, puis EvaluateBuildingStatus :
//                    > 50 % le chantier prend forme, >= 100 % SpawnComposition().
//
// ------------------------------------------------------------------------------------
// POURQUOI ON S'ACCROCHE ICI, ET PAS AILLEURS
//
// Trois points d'accroche etaient possibles. On prend le troisieme :
//
//  1. Un OUTIL a m_iConstructionValue plus eleve (une pelle "du genie").
//     Rejete : le bonus suit l'objet, pas l'homme. La pelle se ramasse, se donne, se
//     pille sur un cadavre -- n'importe qui devient sapeur en se baissant.
//
//  2. Modder GetBuildingToolValue cote CLIENT (le plus simple a ecrire).
//     Rejete : cette fonction tourne chez le joueur, et le serveur avale la valeur
//     RECUE SANS LA VERIFIER (RpcAsk_AddBuildingValue ne controle rien du tout). Laisser
//     le client annoncer son propre rendement, c'est exactement ce qu'on a refuse pour
//     l'accoutumance a l'arme.
//
//  3. >>> Modder RpcAsk_AddBuildingValue cote SERVEUR. <<<
//     Le composant vit sur le PlayerController, donc a la reception on sait de QUI vient
//     la demande sans rien transmettre de plus. Le client continue d'envoyer sa valeur
//     honnete ; c'est le serveur, seul, qui decide du multiplicateur. Un client modifie
//     ne gagne rien de plus qu'avant.
//
// ------------------------------------------------------------------------------------
// PORTEE REELLE -- A LIRE AVANT DE TESTER
//
// Ce bonus couvre TOUT ce qui passe par le systeme de construction de Conflict, donc :
// sacs de sable, barbeles, bunkers, et les 23 batiments FF (FF reutilise ce systeme, nos
// outils portent JWK_ConstructionToolItemComponent).
//
// Il ne couvre PAS le creusement de tranchee ACE, et ce n'est pas un oubli :
// ACE_ShovelUserAction (ACE Core) derive de ACE_ContinousGadgetUserAction, dont tout le
// PerformAction se resume a CancelPlayerAnimation(). C'est une ANIMATION de creusement,
// rien d'autre -- aucun trou, aucune entite, aucune valeur. Et aucun mod ACE Fortify /
// Trenches n'est installe. Il n'y a donc rien a accelerer aujourd'hui. Les tranchees du
// jeu de base (Prefabs/Structures/Military/Fortifications/Trenches) sont du decor pose
// par le concepteur de la carte.
//
// Serveur uniquement. Chaines ASCII (le dedie compile en strict).

// ====================================================================================
// OUTILLAGE COMMUN AUX BONUS DE ROLE
// ====================================================================================

class FFRX_RoleBonus
{
	//------------------------------------------------------------------------------------------------
	//! Identifiant joueur d'une entite personnage, ou 0 si ce n'en est pas une (IA, tourelle...).
	//! Les bonus de role ne concernent QUE des joueurs : un medecin IA garde le comportement vanilla.
	static int PlayerIdOf(IEntity character)
	{
		if (!character)
			return 0;

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return 0;

		return pm.GetPlayerIdFromControlledEntity(character);
	}

	//------------------------------------------------------------------------------------------------
	//! Ce soignant est-il un membre de l'escouade medicale ?
	static bool IsMedicCharacter(IEntity character)
	{
		int pid = PlayerIdOf(character);
		if (pid <= 0)
			return false;

		return FFRX_GroupsManager.IsMedic(pid);
	}
}

// ====================================================================================
// GENIE
// ====================================================================================

modded class SCR_CampaignBuildingNetworkComponent
{
	// Journalise le premier coup de chaque sapeur, pour verifier en jeu que le hook part
	// bien -- sans noyer le log : l'action est repetee plusieurs fois par seconde.
	protected static ref array<int> s_aLoggedPlayers;

	//------------------------------------------------------------------------------------------------
	override protected void RpcAsk_AddBuildingValue(int buildingValue, RplId compId)
	{
		super.RpcAsk_AddBuildingValue(FFRX_ScaleForBuilder(buildingValue), compId);
	}

	//------------------------------------------------------------------------------------------------
	//! Applique le rendement du role au nombre de points annonce par le client.
	protected int FFRX_ScaleForBuilder(int buildingValue)
	{
		if (buildingValue <= 0)
			return buildingValue;

		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache)
			return buildingValue;

		float pct = cache.m_fFFRX_GenieBuild;
		if (pct <= 100)
			return buildingValue;   // reglage a 100 = desactive

		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return buildingValue;

		int playerId = pc.GetPlayerId();
		if (playerId <= 0)
			return buildingValue;

		if (!FFRX_GroupsManager.IsGenie(playerId))
			return buildingValue;

		// Math.Round, pas une troncature : a 110 % un outil a 10 donnerait 11 -> 11.0,
		// mais un outil a 3 donnerait 3.3 -> 3 en tronquant, soit aucun bonus du tout.
		int scaled = Math.Round(buildingValue * pct * 0.01);
		if (scaled <= buildingValue)
			scaled = buildingValue + 1;   // un bonus actif doit toujours se voir

		if (!s_aLoggedPlayers)
			s_aLoggedPlayers = new array<int>();

		if (!s_aLoggedPlayers.Contains(playerId))
		{
			s_aLoggedPlayers.Insert(playerId);
			Print(string.Format("[FFRX][Genie] %1 construit au rendement du genie : %2 -> %3 points par coup (%4 %%).",
				GetGame().GetPlayerManager().GetPlayerName(playerId), buildingValue, scaled, pct));
		}

		return scaled;
	}
}

// ====================================================================================
// SANTE
// ====================================================================================
//
// POURQUOI PAS LE MEME MECANISME QUE LE GENIE. La construction possede un ACCUMULATEUR
// que le serveur detient (m_fCurrentBuildValue) : il suffit d'y verser plus. Le soin n'a
// pas d'equivalent -- il est binaire, le pansement s'applique ou non. Il a fallu trouver
// d'autres prises. Les deux ci-dessous ont en commun d'etre les seuls endroits de la
// chaine de soin ou l'identite du SOIGNANT est disponible cote serveur.
//
// ACE MEDICAL N'EST PAS DANS LA PARTIE (verifie le 2026-09-18) : ACEMedicalCore et
// ACEMedicalCirculation sont installes sur le PC de dev, mais ne sont ni dans notre
// .gproj ni tires par une dependance. Nos deps ACE sont ACE Explosives et ACE
// Carrying-Drag, qui n'amenent qu'ACE Core et ACE Carrying. Le soin des joueurs est donc
// 100 % jeu de base, et c'est bien le jeu de base qu'on mod ici.

//------------------------------------------------------------------------------------------------
//! BONUS 1 -- le pansement d'un medecin regenere davantage.
//!
//! Point d'accroche : AddConsumableDamageEffects recoit l'INSTIGATEUR, c'est-a-dire le
//! soignant. C'est le seul endroit de l'effet de soin ou on sait qui tient la trousse
//! (GetItemRegenSpeed(), lui, ne connait que l'objet).
//!
//! On reimplemente la boucle du parent plutot que d'appeler super : le DPS et la duree
//! sont poses sur l'effet AVANT qu'il ne soit ajoute au gestionnaire de degats, il n'y a
//! pas de retouche possible apres coup.
modded class SCR_ConsumableEffectHealthItems
{
	override void AddConsumableDamageEffects(notnull ChimeraCharacter char, IEntity instigator)
	{
		float mult = 1;

		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (cache && cache.m_fFFRX_MedicHeal > 100 && FFRX_RoleBonus.IsMedicCharacter(instigator))
			mult = cache.m_fFFRX_MedicHeal * 0.01;

		if (mult <= 1)
		{
			super.AddConsumableDamageEffects(char, instigator);
			return;
		}

		SCR_CharacterDamageManagerComponent damageMgr = SCR_CharacterDamageManagerComponent.Cast(char.GetDamageManager());
		if (!damageMgr)
			return;

		array<HitZone> hitZones = {};
		damageMgr.GetHitZonesOfGroup(GetTargetHitZoneGroup(), hitZones);
		if (hitZones.IsEmpty())
			hitZones.Insert(damageMgr.GetDefaultHitZone());

		foreach (SCR_DamageEffect effect : m_aDamageEffectsToLoad)
		{
			effect.SetInstigator(Instigator.CreateInstigator(instigator));
			effect.SetDamageType(effect.GetDefaultDamageType());
			effect.SetAffectedHitZone(hitZones[0]);

			SCR_DotDamageEffect dotClone = SCR_DotDamageEffect.Cast(effect);
			if (dotClone)
			{
				// La regeneration est un degat NEGATIF : d'ou le signe moins, repris du parent.
				// On allonge la duree plutot que d'augmenter le debit -- un soin qui rend tout
				// instantanement supprimerait la fenetre ou le blesse reste vulnerable, qui est
				// justement ce qui rend l'evacuation interessante.
				dotClone.SetDPS(-GetItemRegenSpeed());
				dotClone.SetMaxDuration(m_fItemRegenerationDuration * mult);
			}

			damageMgr.AddDamageEffect(effect);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! BONUS 2 -- un medecin economise parfois son pansement.
//!
//! Point d'accroche : ApplyItemEffect porte le parametre `deleteItem`, et recoit `user`.
//! Passer false laisse l'objet dans la main du soignant ; l'effet, lui, s'applique
//! normalement. C'est du materiel PRESERVE, pas du materiel offert -- le medecin doit
//! toujours partir avec sa trousse pleine.
//!
//! Serveur uniquement : la suppression passe par RplComponent.DeleteRplEntity, c'est donc
//! deja l'autorite qui execute. Le tirage n'est pas reproductible chez le client, il ne
//! peut pas le forcer.
modded class SCR_ConsumableItemComponent
{
	override void ApplyItemEffect(IEntity target, IEntity user, ItemUseParameters animParams, IEntity item, bool deleteItem = true)
	{
		if (deleteItem)
		{
			JWK_GameSettingsCache cache = JWK.GameSettingsCache();
			if (cache && cache.m_fFFRX_MedicSave > 0 && FFRX_RoleBonus.IsMedicCharacter(user))
			{
				if (JWK.Random.RandFloat01() * 100 < cache.m_fFFRX_MedicSave)
					deleteItem = false;
			}
		}

		super.ApplyItemEffect(target, user, animParams, item, deleteItem);
	}
}

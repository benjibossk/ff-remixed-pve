// FF - REMIXED - PVE
// Persistent civilian identities + TRUST (Pillar 4 -- RPG civilians).
//
// Each generic civilian gets a DETERMINISTIC identity (name) anchored to where it lives (a ~5 m
// cell), so you always meet "Ivan Kovac" at the same spot in the same town. On top of the identity,
// each civilian carries a persistent TRUST level (0-100, 50 = neutral) + interaction count, stored
// in JSON ($profile:FFRX_civilians.json) so it survives despawn AND server restarts.
//
// Trust is moved by interactions (MCD dialogue: help/gift = +, extort/hostile = -; frisk innocent
// = -). PHASE 1 = identity binding (done). PHASE 2 (here) = trust + persistence. Wiring to the
// dialogue/frisk comes next.

class FFRX_CivIdentity
{
	string       m_sKey;          // identifiant stable de la PERSONNE (jamais reutilise)
	string       m_sZone;         // zone d'appartenance ("E4") -- c'est LA l'ancrage
	string       m_sName;         // deterministic display name
	ResourceName m_sPrefab;       // deterministic face (unused in A1 -- FF keeps the face)
	float        m_fTrust;        // 0..100, 50 = neutral
	int          m_iInteractions; // how many times interacted
	// Controles d'identite (fouilles). Comptes ici et PERSISTES : c'est ce qui permet au
	// site de montrer qui a deja ete controle, et combien de ces controles ont donne un espion.
	int          m_iFrisks;       // fouilles subies
	int          m_iSpyFound;     // dont fouilles ayant demasque un espion
}

// --- JSON persistence shape ---
class FFRX_CivSaveEntry
{
	string key;
	string name;
	float  trust;
	int    interactions;
	// Ajouts 2026-09-18. Les anciennes sauvegardes n'ont pas ces champs : ReadValue les
	// laisse simplement a 0, donc le fichier existant reste lisible sans migration.
	int    frisks;
	int    spyFound;
	// Ajouts 2026-09-18 (bascule case -> zone). `zone` est l'ancrage ; x/z ne servent
	// qu'a placer la fiche sur la carte du site (derniere position vue).
	string zone;
	float  x;
	float  z;
}

class FFRX_CivSaveFile
{
	ref array<ref FFRX_CivSaveEntry> civs;
	void FFRX_CivSaveFile() { civs = {}; }
}

class FFRX_CivIdentityRegistry
{
	protected static ref FFRX_CivIdentityRegistry s_Inst;

	static FFRX_CivIdentityRegistry Get()
	{
		if (!s_Inst)
		{
			s_Inst = new FFRX_CivIdentityRegistry();
			s_Inst.FFRX_Load();
		}
		return s_Inst;
	}

	protected static const string SAVE_PATH     = "$profile:FFRX_civilians.json";
	protected static const float  FFRX_CELL     = 5.0;   // ancien ancrage, garde pour la conversion
	protected static const float  ZONE_M        = 1000.0; // cote d'une zone, en metres
	protected static const float  TRUST_DEFAULT = 50.0;

	protected ref map<string, ref FFRX_CivIdentity>   m_ByKey    = new map<string, ref FFRX_CivIdentity>();
	protected ref map<EntityID, ref FFRX_CivIdentity> m_ByEntity = new map<EntityID, ref FFRX_CivIdentity>();
	protected ref map<string, ref FFRX_CivSaveEntry>  m_Saved    = new map<string, ref FFRX_CivSaveEntry>();
	//! Habitants actuellement INCARNES par un civil vivant. Empeche deux PNJ de porter le
	//! meme nom en meme temps. Vide au demarrage : aucun civil n'existe encore.
	protected ref map<string, bool> m_BoundIds = new map<string, bool>();
	//! Compteur de personnes, pour fabriquer des identifiants uniques a vie.
	protected int m_iNextPerson;
	protected bool m_bDirty;

	protected ref array<string> m_First = { "Ivan", "Petr", "Milos", "Sacha", "Goran", "Luka", "Anton", "Dragan", "Nikolai", "Marko", "Emil", "Radan" };
	protected ref array<string> m_Last  = { "Kovac", "Petrov", "Horvat", "Novak", "Ilic", "Sokolov", "Babic", "Volkov", "Maric", "Popov", "Juric", "Tomic" };
	protected ref array<ResourceName> m_Faces = {
		"{8C7093AF368F496A}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_CottonShirt_1.et",
		"{DF7F8D5C05CC1AF6}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_CottonShirt_2.et",
		"{11EB9A0D2A5899EA}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_DenimJacket_1.et",
		"{C943F3CC53D187B6}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_Turtleneck_1.et",
		"{E024A74F8A4BC644}Prefabs/Characters/Factions/CIV/Businessman/Character_CIV_Businessman_1.et"
	};

	//------------------------------------------------------------------------------------------------
	//  ANCRAGE : UNE POPULATION PAR ZONE (2026-09-18)
	//
	//  AVANT : l'identite etait derivee de la CASE de 5 m ou on voyait le civil pour la premiere
	//  fois. Consequence, l'identite appartenait au LIEU : le type pres du puits etait toujours
	//  Ivan Kovac, mais un civil apparu trois metres plus loin etait quelqu'un d'autre. Il n'y
	//  avait pas d'habitants, seulement des cases memorisees.
	//
	//  MAINTENANT : chaque ZONE (carre d'1 km, "E4") possede une liste d'HABITANTS. Quand on
	//  croise un civil, on lui attribue un habitant de sa zone qui n'est pas deja incarne ; s'il
	//  n'y en a pas de libre, la zone gagne un habitant de plus. Les memes visages reviennent donc
	//  dans leur village avec leur memoire.
	//
	//  CE QUE CA NE FAIT PAS, ET POURQUOI. On ne controle toujours PAS l'apparition des civils :
	//  c'est FF qui les fait naitre et disparaitre, et reprendre ce spawn avait casse la build
	//  (cf. en-tete de FFRX_CivRoster.c). On ne peut donc pas garantir qu'un habitant donne soit
	//  present a un instant donne, ni recomposer le meme groupe. Ce qu'on garantit : un civil
	//  croise dans une zone est TOUJOURS quelqu'un de cette zone.
	//------------------------------------------------------------------------------------------------

	//! Etiquette de zone, facon carroyage militaire : "E4". Lettre = colonne (X), chiffre = ligne (Z).
	//! 1 km : un village tient dans une case, deux localites restent distinctes.
	//! PUBLIQUE et statique : FFRX_CivSender s'en sert pour etiqueter les fiches envoyees au site.
	//! Une seule definition, donc jeu et site ne peuvent pas diverger.
	static string ZoneLabel(vector pos)
	{
		int cx = (int)Math.Floor(pos[0] / ZONE_M);
		int cz = (int)Math.Floor(pos[2] / ZONE_M);

		if (cx < 0 || cz < 0 || cx >= 26)
			return "hors carte";

		string letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		return letters.Substring(cx, 1) + cz.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Attribue un habitant de la zone de 'pos' : un deja connu et libre, sinon un nouveau.
	FFRX_CivIdentity GetForZone(vector pos)
	{
		string zone = ZoneLabel(pos);

		// 1) Un habitant DEJA CONNU de cette zone, pas encore incarne ? On le reprend : c'est
		//    tout l'interet du systeme -- sa confiance et son historique reviennent avec lui.
		foreach (string k, FFRX_CivSaveEntry e : m_Saved)
		{
			if (!e || e.zone != zone)
				continue;
			if (m_BoundIds.Contains(e.key))
				continue;

			FFRX_CivIdentity known = m_ByKey.Get(e.key);
			if (!known)
			{
				known = BuildIdentity(e.key, zone);
				known.m_fTrust        = e.trust;
				known.m_iInteractions = e.interactions;
				known.m_iFrisks       = e.frisks;
				known.m_iSpyFound     = e.spyFound;
				if (e.name != "")
					known.m_sName = e.name;   // le nom sauve prime : il a deja ete vu par les joueurs
				m_ByKey.Set(e.key, known);
			}
			return known;
		}

		// 2) Personne de libre : la zone gagne un habitant. La cle inclut un compteur, donc elle
		//    est unique a vie -- on ne reutilise JAMAIS l'identifiant d'une personne, sinon deux
		//    habitants distincts se partageraient une confiance.
		m_iNextPerson++;
		string key = zone + "#" + m_iNextPerson.ToString();

		FFRX_CivIdentity id = BuildIdentity(key, zone);
		id.m_fTrust        = TRUST_DEFAULT;
		id.m_iInteractions = 0;
		m_ByKey.Set(key, id);
		return id;
	}

	//! Nom et visage deterministes a partir de la cle : deux serveurs partant du meme fichier
	//! affichent le meme habitant.
	protected FFRX_CivIdentity BuildIdentity(string key, string zone)
	{
		int h = FFRX_HashStr(key);
		if (h < 0)
			h = -h;

		FFRX_CivIdentity id = new FFRX_CivIdentity();
		id.m_sKey    = key;
		id.m_sZone   = zone;
		id.m_sName   = m_First[h % m_First.Count()] + " " + m_Last[(h / 7) % m_Last.Count()];
		id.m_sPrefab = m_Faces[(h / 13) % m_Faces.Count()];
		return id;
	}

	void Bind(EntityID ent, FFRX_CivIdentity id)
	{
		if (!id)
			return;

		m_ByEntity.Set(ent, id);
		// Marque l'habitant comme INCARNE : tant que ce civil vit, personne d'autre ne peut
		// etre "lui". Sans ca, deux civils de la meme zone porteraient le meme nom en meme temps.
		m_BoundIds.Set(id.m_sKey, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Libere les habitants dont le civil a disparu (despawn FF, mort). Appele par le scanner.
	//! Sans ce menage, une zone epuiserait ses habitants libres et en creerait sans fin.
	void PruneDead()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		array<EntityID> gone = {};
		foreach (EntityID eid, FFRX_CivIdentity id : m_ByEntity)
		{
			if (!world.FindEntityByID(eid))
				gone.Insert(eid);
		}

		foreach (EntityID eid : gone)
		{
			FFRX_CivIdentity id = m_ByEntity.Get(eid);
			if (id)
				m_BoundIds.Remove(id.m_sKey);
			m_ByEntity.Remove(eid);
		}
	}

	FFRX_CivIdentity IdentityOf(IEntity ent)
	{
		if (!ent)
			return null;
		return m_ByEntity.Get(ent.GetID());
	}

	// Identity of a civ entity, creating one (by position) if not bound yet.
	FFRX_CivIdentity IdentityOfOrCreate(IEntity ent)
	{
		if (!ent)
			return null;
		FFRX_CivIdentity id = m_ByEntity.Get(ent.GetID());
		if (!id)
		{
			id = GetForZone(ent.GetOrigin());
			Bind(ent.GetID(), id);
		}
		return id;
	}

	//------------------------------------------------------------------------------------------------
	// Move trust for an identity (clamped 0..100) and persist it.
	void AddTrust(FFRX_CivIdentity id, float delta)
	{
		if (!id)
			return;
		id.m_fTrust        = Math.Clamp(id.m_fTrust + delta, 0.0, 100.0);
		id.m_iInteractions = id.m_iInteractions + 1;

		FFRX_CivSaveEntry e = m_Saved.Get(id.m_sKey);
		if (!e)
		{
			e = new FFRX_CivSaveEntry();
			e.key = id.m_sKey;
			m_Saved.Set(id.m_sKey, e);
		}
		e.name         = id.m_sName;
		e.trust        = id.m_fTrust;
		e.interactions = id.m_iInteractions;
		e.zone     = id.m_sZone;
		m_bDirty = true;
	}

	// Convenience for callers holding a civ entity (dialogue / frisk).
	void AddTrustForEntity(IEntity civ, float delta)
	{
		AddTrust(IdentityOfOrCreate(civ), delta);
	}

	//------------------------------------------------------------------------------------------------
	//! Note ou cet habitant vient d'etre apercu. Sert UNIQUEMENT a placer sa fiche sur la carte
	//! du site : la position n'est plus l'ancrage de l'identite, c'est la zone qui l'est.
	//!
	//! N'ecrit QUE si une fiche existe deja : on ne veut pas creer une fiche pour chaque civil
	//! simplement apercu, sinon le fichier se remplirait de gens a qui personne n'a jamais parle.
	void NoteSeenAt(FFRX_CivIdentity id, vector pos)
	{
		if (!id)
			return;

		FFRX_CivSaveEntry e = m_Saved.Get(id.m_sKey);
		if (!e)
			return;

		e.x = pos[0];
		e.z = pos[2];
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Enregistre un CONTROLE D'IDENTITE (fouille) sur ce civil.
	//!
	//! Separe d'AddTrust volontairement : une fouille modifie la confiance (via AddTrust) MAIS
	//! est aussi un fait de renseignement en soi -- "ce civil a deja ete controle trois fois"
	//! est une information que l'etat-major veut voir, independamment de ce que la confiance
	//! est devenue. Les deux compteurs vivent donc cote a cote.
	void NoteFrisk(FFRX_CivIdentity id, bool spyFound)
	{
		if (!id)
			return;

		id.m_iFrisks = id.m_iFrisks + 1;
		if (spyFound)
			id.m_iSpyFound = id.m_iSpyFound + 1;

		FFRX_CivSaveEntry e = m_Saved.Get(id.m_sKey);
		if (!e)
		{
			e = new FFRX_CivSaveEntry();
			e.key = id.m_sKey;
			m_Saved.Set(id.m_sKey, e);
		}
		e.name     = id.m_sName;
		e.trust    = id.m_fTrust;
		e.frisks   = id.m_iFrisks;
		e.spyFound = id.m_iSpyFound;
		e.zone     = id.m_sZone;
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Copie du fichier civil, pour l'emission vers le site (FFRX_CivSender).
	//!
	//! On rend `m_Saved`, c'est-a-dire les civils avec qui on a REELLEMENT interagi -- pas
	//! `m_ByKey`, qui contient toute cellule simplement traversee pendant la session et qui
	//! gonflerait sans rien apprendre. "Rencontre" veut dire "on lui a parle ou on l'a
	//! fouille", ce qui est exactement ce qu'on veut lire sur la page.
	void FFRX_Snapshot(notnull array<ref FFRX_CivSaveEntry> outList)
	{
		foreach (string k, FFRX_CivSaveEntry e : m_Saved)
			if (e)
				outList.Insert(e);
	}

	// Trust of a civ entity (0..100, default 50 = neutral if unknown).
	float TrustOf(IEntity civ)
	{
		FFRX_CivIdentity id = IdentityOfOrCreate(civ);
		if (!id)
			return TRUST_DEFAULT;
		return id.m_fTrust;
	}

	// Multiplier for a POSITIVE outcome chance: trust/50 (x0 at 0, x1 at 50 neutral, x2 at 100).
	float TrustScalePos(IEntity civ)
	{
		return TrustOf(civ) / 50.0;
	}

	// Multiplier for a NEGATIVE outcome chance: (100-trust)/50 (x2 at 0, x1 at 50, x0 at 100).
	float TrustScaleNeg(IEntity civ)
	{
		return (100.0 - TrustOf(civ)) / 50.0;
	}

	//------------------------------------------------------------------------------------------------
	void FFRX_Save()
	{
		if (!m_bDirty)
			return;

		FFRX_CivSaveFile f = new FFRX_CivSaveFile();
		foreach (string k, FFRX_CivSaveEntry e : m_Saved)
			f.civs.Insert(e);

		SCR_JsonSaveContext ctx = new SCR_JsonSaveContext();
		ctx.WriteValue("", f);
		ctx.SaveToFile(SAVE_PATH);
		m_bDirty = false;
		Print(string.Format("[FFRX][Civ] %1 fiches civiles sauvees.", f.civs.Count()), LogLevel.NORMAL);
	}

	protected void FFRX_Load()
	{
		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext();
		if (!ctx.LoadFromFile(SAVE_PATH))
			return;

		FFRX_CivSaveFile f = new FFRX_CivSaveFile();
		if (!ctx.ReadValue("", f))
			return;

		int converted = 0;
		foreach (FFRX_CivSaveEntry e : f.civs)
		{
			if (!e || e.key == "")
				continue;

			// CONVERSION des fiches d'AVANT la bascule case -> zone (2026-09-18).
			//
			// Une fiche ancienne a une cle de CASE ("1234_-567") et pas de zone. On lui calcule
			// sa zone depuis cette case -- l'ancienne cle porte la position, c'est ce qui rend la
			// conversion possible sans rien perdre. La cle, elle, ne change PAS : elle reste
			// l'identifiant unique de la personne, qui devient simplement habitante de sa zone.
			//
			// Sans ca, toutes les confiances et tous les controles deja gagnes seraient orphelins :
			// aucune fiche n'ayant de zone, aucune ne serait jamais reattribuee.
			if (e.zone == "")
			{
				int sep = e.key.IndexOf("_");
				if (sep > 0)
				{
					string sx = e.key.Substring(0, sep);
					string sz = e.key.Substring(sep + 1, e.key.Length() - sep - 1);
					vector p = Vector(sx.ToInt() * FFRX_CELL, 0, sz.ToInt() * FFRX_CELL);
					e.zone = ZoneLabel(p);
					e.x = p[0];
					e.z = p[2];
					converted++;
					m_bDirty = true;   // la conversion sera reecrite au prochain enregistrement
				}
			}

			m_Saved.Set(e.key, e);
		}

		if (converted > 0)
			Print(string.Format("[FFRX][Civ] %1 anciennes fiches (ancrage par case) converties en habitants de zone.", converted), LogLevel.NORMAL);

		Print(string.Format("[FFRX][Civ] %1 fiches civiles chargees.", m_Saved.Count()), LogLevel.NORMAL);
	}

	// Stable string hash (avoid string.Hash which is unreliable in Enforce).
	protected int FFRX_HashStr(string s)
	{
		int h = 0;
		int n = s.Length();
		for (int i = 0; i < n; i++)
			h = h + s.ToAscii(i) * (i + 1);
		return h;
	}
}

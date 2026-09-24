// FF - REMIXED - PVE
// Retire l'outil carte "Map display settings" (FF) aux non-admins : le bouton de la
// barre laterale ET le panneau qu'il ouvre.
//
// ------------------------------------------------------------------------------------
// LE PROBLEME : UN MENU QUI MENT
//
// Cet outil propose des interrupteurs pour les couches de la carte -- Points of interest,
// Territory control, Radio signal, FOB placement, Camp placement, Illegal zones. C'est
// exactement ce que `FFRX_EmptyMap` eteint pour le brouillard de guerre.
//
// Le brouillard n'est PAS cassable : `FFRX_EmptyMap` surcharge `SetVisible` sur
// `JWK_BaseMapModule` et bloque toute remise en visibilite pour un non-admin, meme si
// l'interface la demande. Le danger n'est donc pas la triche, c'est la CONFUSION -- un
// joueur bascule un interrupteur, rien ne se passe, et il conclut a un bug du serveur.
//
// On supprime donc l'outil au lieu de laisser des boutons inertes. Un reglage qui ne
// fait rien est pire que pas de reglage : il fait douter de tout le reste.
//
// ------------------------------------------------------------------------------------
// OU ON SE BRANCHE, ET POURQUOI ICI
//
// Deux accroches etaient possibles :
//
//   - `JWK_MapDisplaySettingsUIComponent` (le handler du widget du panneau) -- ca cache
//     le panneau, mais le BOUTON JAUNE de la barre laterale reste. Le joueur clique dans
//     le vide. Rejete.
//
//   - `JWK_MapDisplaySettingsUI` (le composant carte) -- c'est lui qui appelle
//     `RegisterToolMenuEntry` dans `Init()`. Ne pas enregistrer l'entree = pas de bouton,
//     donc pas de panneau. On coupe A LA SOURCE. Retenu.
//
// La regle d'acces vient de `FFRX_MapAccess.LocalPlayerSeesMap()`, la MEME que celle du
// brouillard : une seule definition de "qui voit la carte", pour que les deux suivent
// ensemble si elle change. Dupliquer le test ici aurait garanti une divergence.
//
// C'est de l'UI CLIENT, donc cosmetique : l'autorite reste le verrou de FFRX_EmptyMap.
// Retirer l'outil ne remplace pas ce verrou, ca le complete.
//
// NOTE : ASCII uniquement dans les chaines/commentaires (le build du dedie desynchronise
// sur l'UTF-8).

modded class JWK_MapDisplaySettingsUI
{
	override void Init()
	{
		// Non-admin : on n'enregistre RIEN. `m_ToolMenuEntry` reste null, ce dont
		// `OnMapOpen` ci-dessous tient compte.
		if (!FFRX_MapAccess.LocalPlayerSeesMap())
			return;

		super.Init();
	}

	override protected void OnMapOpen(MapConfiguration config)
	{
		if (!FFRX_MapAccess.LocalPlayerSeesMap())
			return;   // sans Init(), `m_wWidget` est null -> le super planterait

		// Rattrapage : le statut admin peut arriver APRES le `Init()` de la carte (il
		// depend du serveur). Sans ca, un admin connecte tot perdrait son bouton jusqu'a
		// la fin de la partie. Garde sur `m_ToolMenuEntry` pour ne pas enregistrer deux
		// fois -- ca ferait deux boutons identiques dans la barre.
		if (!m_ToolMenuEntry)
			super.Init();

		super.OnMapOpen(config);
	}
}

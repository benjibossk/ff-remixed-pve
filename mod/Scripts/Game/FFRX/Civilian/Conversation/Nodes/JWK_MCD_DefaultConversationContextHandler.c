// CE FICHIER EST INTENTIONNELLEMENT VIDE.
// Le modded JWK_DefaultConversationContextHandler a été supprimé car InitContext
// retourne bool et notre override retournait implicitement false, cassant toute conversation.
// Le reset du GreetRegistry est maintenant fait par JWK_MCD_ResetGreetNode
// placé comme premier nœud dans Talk_AmbientCivilian.conf.

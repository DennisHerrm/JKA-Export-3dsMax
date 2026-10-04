-- ============================================================
--  JkaMenu_2025_2027.ms - Menue "JKA Tool" ab 3ds Max 2025 (pre-start-up,
--  Callback #cuiRegisterMenus, wie beim SWBF2/KOTOR Import). Eigene GUIDs.
-- ============================================================
(
  local sSubMenuGuid = "838F8FCE-82BB-4457-AD97-BE9DF800548A"
  local sActionGuid  = "3BC44CEC-DA26-4849-9C9F-E03E859DDE51"
  fn JkaRegisterMenu =
  (
    local mgr = callbacks.notificationParam()
    if (mgr != undefined) then
    (
      try
      (
        local mnu = mgr.mainMenuBar.CreateSubMenu sSubMenuGuid "JKA Tool"
        if (mnu != undefined) then mnu.CreateAction sActionGuid 647394 "JkaExport_Open`JKA Tool"
      )
      catch (format "[JKA Tool] menu: %\n" (getCurrentException()))
    )
  )
  callbacks.removeScripts id:#jkaExportMenu
  callbacks.addScript #cuiRegisterMenus JkaRegisterMenu id:#jkaExportMenu
)

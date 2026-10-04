-- ============================================================
--  JkaMenu_2016_2024.ms - Menue "JKA Tool" bis 3ds Max 2024 (menuMan, post-start-up)
-- ============================================================
(
  local mnu = menuMan.findMenu "JKA Tool"
  if (mnu == undefined) then
  (
    local bar = menuMan.getMainMenuBar()
    mnu = menuMan.createMenu "JKA Tool"
    bar.addItem (menuMan.createSubMenuItem "JKA Tool" mnu) (bar.numItems())
  )
  local bDa = false
  for i = 1 to mnu.numItems() do if ((mnu.getItem i).getTitle() == "Export GLM/GLA") do bDa = true
  if (not bDa) then
  (
    local a = menuMan.createActionItem "JkaExport_Open" "JKA Tool"
    a.setTitle "Export GLM/GLA"
    a.setUseCustomTitle true
    mnu.addItem a -1
  )
  menuMan.updateMenuBar()
)

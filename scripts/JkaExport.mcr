-- ============================================================
--  JkaExport.mcr - Menue "JKA Tool" -> "Export GLM/GLA"
-- ============================================================
macroScript JkaExport_Open
    category:"JKA Tool"
    buttonText:"Export GLM/GLA"
    toolTip:"Export the selected character as Jedi Academy Ghoul2 model (GLM + GLA + animation.cfg)"
(
    on execute do
    (
        local o = undefined
        try (if (JkaExportCpp != undefined) then o = JkaExportCpp) catch (o = undefined)
        if (o == undefined) then
            try (for i in getCoreInterfaces() where matchPattern (i as string) pattern:"*JkaExportCpp*" do o = i) catch ()
        if (o != undefined) then o.showDialog()
        else messageBox "JkaExport.dle is not loaded - run BUILD.bat / INSTALLIERE.bat and restart 3ds Max." title:"JKA Tool"
    )
)

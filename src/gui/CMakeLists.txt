file(GLOB gui_HDRS "*.h")
file(GLOB clay_HDRS "clay/*.h")

set(gui_SRCS
        ${gui_HDRS}
        ${clay_HDRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/guiAnimatedImage.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiBackgroundImage.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiBox.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiButton.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiButtonImage.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiButtonItemImage.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiButtonKey.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiChatConsole.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/statusTextHelper.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiEditBoxWithScrollbar.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiEngine.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiFormSpecMenu.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiInventoryList.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiItemImage.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiOpenURL.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiPasswordChange.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiPathSelectMenu.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiScene.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiScrollBar.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiScrollContainer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiTable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiHyperText.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiVolumeChange.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/modalMenu.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/profilergraph.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/touchcontrols.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/touchscreenlayout.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/touchscreeneditor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/drawItemStack.cpp
        # Clay UI integration (v9.46)
        ${CMAKE_CURRENT_SOURCE_DIR}/clay/clay_irrlicht_renderer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/clay/clay_gui_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/clay/clay_text_measurer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/clay/clay_event_bridge.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/clay/clay_ui_pause_menu.cpp
        PARENT_SCOPE
)

SET(PACKAGE_NAME HELICORDER)
SET(APP_NAME scheli)

SET(
	${PACKAGE_NAME}_SOURCES
		main.cpp
		mainwindow.cpp
		heliwidget.cpp
)

SET(
	${PACKAGE_NAME}_HEADERS
)


SET(
	${PACKAGE_NAME}_MOC_HEADERS
		mainwindow.h
		heliwidget.h
		app.h
)

SET(
	${PACKAGE_NAME}_UI
		mainwindow.ui
)


SET(
	${PACKAGE_NAME}_RESOURCES
)

SC_ADD_GUI_EXECUTABLE(${PACKAGE_NAME} ${APP_NAME})
SC_LINK_LIBRARIES_INTERNAL(${APP_NAME} qt)

SC_INSTALL_INIT(${APP_NAME} ${INIT_ND_NB_TEMPLATE})

FILE(GLOB descs "${CMAKE_CURRENT_SOURCE_DIR}/descriptions/*.xml")
INSTALL(FILES ${descs} DESTINATION ${SC3_PACKAGE_APP_DESC_DIR})

IF (SC_GLOBAL_GUI_QT5)
	FIND_PACKAGE(Qt5 REQUIRED PrintSupport)
	SC_LINK_LIBRARIES(${APP_NAME} Qt5::PrintSupport)
ELSEIF (SC_GLOBAL_GUI_QT6)
	FIND_PACKAGE(Qt6 REQUIRED PrintSupport)
	SC_LINK_LIBRARIES(${APP_NAME} Qt6::PrintSupport)
ENDIF()




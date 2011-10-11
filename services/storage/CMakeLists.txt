project(storage)

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=300105)

include_directories(
  ${SOPRANO_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}
  )

set(storage_SRCS
  storage.cpp
  nepomukcore.cpp
  modelcopyjob.cpp
  repository.cpp
  ontologyloader.cpp
  ontologymanagermodel.cpp
  graphretriever.cpp
  crappyinferencer.cpp
  crappyinferencer2.cpp
  removablemediamodel.cpp
  datamanagementmodel.cpp
  datamanagementadaptor.cpp
  datamanagementcommand.cpp
  classandpropertytree.cpp
  resourcemerger.cpp
  resourceidentifier.cpp
  resourcewatchermanager.cpp
  resourcewatcherconnection.cpp
  graphmaintainer.cpp
  )

qt4_add_dbus_adaptor(storage_SRCS
  ../../interfaces/org.kde.nepomuk.OntologyManager.xml
  ontologyloader.h
  Nepomuk::OntologyLoader)

qt4_add_dbus_adaptor(storage_SRCS
  ../../interfaces/org.kde.nepomuk.ResourceWatcherConnection.xml
  resourcewatcherconnection.h
  Nepomuk::ResourceWatcherConnection)

kde4_add_plugin(nepomukstorage ${storage_SRCS})

target_link_libraries(nepomukstorage
  ${SOPRANO_LIBRARIES}
  ${SOPRANO_SERVER_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
  ${KDE4_KIO_LIBS}
  ${KDE4_SOLID_LIBS}
  ${NEPOMUK_LIBRARIES}
  nepomukcore
  nepomukcommon
  nepomuksync
  )

install(
  FILES nepomukstorage.desktop
  DESTINATION ${SERVICES_INSTALL_DIR})

install(
  FILES nepomukstorage.notifyrc
  DESTINATION ${DATA_INSTALL_DIR}/nepomukstorage)

install(
  TARGETS nepomukstorage
  DESTINATION ${PLUGIN_INSTALL_DIR})
# -----------------------------

add_subdirectory(rcgen)
add_subdirectory(test)
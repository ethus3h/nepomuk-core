project(nepomuk-show)

include_directories(
  ${QT_INCLUDES}
  ${KDE4_INCLUDES}
  ${SOPRANO_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}
  ${NEPOMUK_CORE_INCLUDE_DIR}
  )

set( SRCS
  main.cpp
  resourceprinter.cpp
  uri.cpp
)

kde4_add_executable(nepomukshow ${SRCS})

target_link_libraries(nepomukshow
  ${KDE4_KDECORE_LIBS}
  ${SOPRANO_LIBRARIES}
  nepomukcore
  )

install(TARGETS nepomukshow
        DESTINATION ${BIN_INSTALL_DIR})

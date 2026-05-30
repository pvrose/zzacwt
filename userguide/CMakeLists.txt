#[=[
	Copyright 2026, Philip Rose, GM3ZZA
	
    This file is part of ZZACWT. Amateur Radio Logging Software.

    ZZACWT is free software: you can redistribute it and/or modify it under the
	terms of the Lesser GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later version.

    ZZACWT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
	without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
	PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along with ZZACWT. 
	If not, see <https://www.gnu.org/licenses/>. 

#]=]
find_package(Doxygen)

if(DOXYGEN_FOUND)

  message(STATUS "Doxygen found: ${DOXYGEN_EXECUTABLE}")

  # Set project files
  set(DOXY_UG_INDEX_FILE index.html)
  set(DOXY_UG_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
  set(DOXY_UG_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_ug)

  set(DOXY_UG_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/data.dox
    ${CMAKE_CURRENT_SOURCE_DIR}/src/generation.dox
    ${CMAKE_CURRENT_SOURCE_DIR}/src/index.dox
    ${CMAKE_CURRENT_SOURCE_DIR}/src/overview.dox
    ${CMAKE_CURRENT_SOURCE_DIR}/src/review.dox
    ${CMAKE_CURRENT_SOURCE_DIR}/new_header.tex
  )

  set(DOXY_UG_IMAGES 
    ${CMAKE_CURRENT_SOURCE_DIR}/images/review.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/user_if.png
  )



  # Copy Doxyfile
  configure_file(${DOXY_UG_IN} ${DOXY_UG_OUT} @ONLY)
 
  # Command for generating dox from config file
  message("Adding command to generate: ${CMAKE_CURRENT_BINARY_DIR}/html/index.html")
  
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/html/index.html
    DEPENDS ${DOXY_UG_FILES} ${DOXY_UG_OUT} ${DOXY_UG_IMAGES}
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXY_UG_OUT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "ZZACWT: Generating userguide"
    VERBATIM
  )
  add_custom_target(ug_html ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/html/index.html
  )
  
  if(ZZACWT_PDF)
      # Command for generating PDF from userguide latex files
      if(MSVC)
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/latex/refman.pdf
            DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/html/index.html
                    ug_html
            COMMAND miktex-texworks.exe  ${CMAKE_CURRENT_BINARY_DIR}/latex/refman.tex
            COMMENT "ZZACWT: Generating PDF Userguide"
            VERBATIM
        )
      else()
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/latex/refman.pdf
            DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/html/index.html
                    ug_html
            COMMAND make -C ${CMAKE_CURRENT_BINARY_DIR}/latex >> /dev/null
            COMMENT "ZZACWT: Generating PDF Userguide"
            VERBATIM
        )
      endif()
      add_custom_target(ug_pdf ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/latex/refman.pdf
      )
      # Copy from latex/refman.pdf to ZZACWT.pdf
      add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.pdf
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/latex/refman.pdf
                ug_pdf
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/latex/refman.pdf
        ${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.pdf
        COMMENT "Copying PDF"
      )
      add_custom_target(
        pdf ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.pdf
      )
  endif()
endif()

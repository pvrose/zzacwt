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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio.dox
    ${CMAKE_CURRENT_SOURCE_DIR}/src/data.dox
    ${CMAKE_CURRENT_SOURCE_DIR}/src/decoder.dox
    ${CMAKE_CURRENT_SOURCE_DIR}/src/generation.dox
    ${CMAKE_CURRENT_SOURCE_DIR}/src/index.dox
    ${CMAKE_CURRENT_SOURCE_DIR}/src/overview.dox
    ${CMAKE_CURRENT_SOURCE_DIR}/src/review.dox
    ${CMAKE_CURRENT_SOURCE_DIR}/src/release_notes.dox
    ${CMAKE_CURRENT_SOURCE_DIR}/new_header.tex
  )

  set(DOXY_UG_IMAGES 
    ${CMAKE_CURRENT_SOURCE_DIR}/images/decoder.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/review.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/user_if.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/user_if_2.png
  )

  setup_ug_documentation(
      ZZACWT
      ${DOXY_UG_FILES}
      ${DOXY_UG_IMAGES}
  )

  
  # Set output directory for API HTML
  set(ZZACWT_USERGUIDE_HTML_DIR "${CMAKE_CURRENT_BINARY_DIR}/html" PARENT_SCOPE)
  set(ZZACWT_USERGUIDE_PDF_FILE "${CMAKE_CURRENT_BINARY_DIR}/ZZACWT.pdf" PARENT_SCOPE)



endif()

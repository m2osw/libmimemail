# Copyright (c) 2011-2022  Made to Order Software Corp.  All Rights Reserved
#
# https://snapwebsites.org/project/libmimemail
# contact@m2osw.com
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

project(mimemail)

AtomicNames("names.an")

# Put the version in the header file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
)

add_library(${PROJECT_NAME} SHARED
    attachment.cpp
    email.cpp
    mail_exchanger.cpp
    names.cpp
    version.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${SNAPLOGGER_INCLUDE_DIR}
        ${SNAPDEV_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ${SNAPLOGGER_LIBRARIES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION
        ${LIBMIMEMAIL_VERSION_MAJOR}.${LIBMIMEMAIL_VERSION_MINOR}

    SOVERSION
        ${LIBMIMEMAIL_VERSION_MAJOR}
)

install(
    TARGETS
        ${PROJECT_NAME}

    LIBRARY DESTINATION
        lib
)

install(
    DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}/

    DESTINATION
        include/libmimemail

    FILES_MATCHING PATTERN
        "*.h"
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/version.h

    DESTINATION
        include/libmimemail
)


# vim: ts=4 sw=4 et nocindent

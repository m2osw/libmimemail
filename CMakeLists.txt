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

cmake_minimum_required(VERSION 3.10.2)

project(libmimemail_project)

find_package(AtomicNames      REQUIRED)
find_package(CppProcess       REQUIRED)
find_package(CppThread        REQUIRED)
find_package(EdHttp           REQUIRED)
find_package(LibExcept        REQUIRED)
find_package(LibTLD           REQUIRED)
find_package(SnapCMakeModules REQUIRED)
find_package(SnapDev          REQUIRED)
find_package(SnapLogger       REQUIRED)

SnapGetVersion(LIBMIMEMAIL ${CMAKE_CURRENT_SOURCE_DIR})

enable_language(CXX)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_BINARY_DIR}
)

##
## Compiling
##
add_subdirectory(libmimemail)       # The libmimemail library
add_subdirectory(cmake)             # CMake Config
add_subdirectory(doc)               # Documentation

# vim: ts=4 sw=4 et nocindent

# - Find LibMimeMail
#
# LIBMIMEMAIL_FOUND        - System has LibMimeMail
# LIBMIMEMAIL_INCLUDE_DIRS - The LibMimeMail include directories
# LIBMIMEMAIL_LIBRARIES    - The libraries needed to use LibMimeMail
# LIBMIMEMAIL_DEFINITIONS  - Compiler switches required for using LibMimeMail
#
# License:
#
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

find_path(
    LIBMIMEMAIL_INCLUDE_DIR
        libmimemail/version.h

    PATHS
        ENV LIBMIMEMAIL_INCLUDE_DIR
)

find_library(
    LIBMIMEMAIL_LIBRARY
        mimemail

    PATHS
        ${LIBMIMEMAIL_LIBRARY_DIR}
        ENV LIBMIMEMAIL_LIBRARY
)

mark_as_advanced(
    LIBMIMEMAIL_INCLUDE_DIR
    LIBMIMEMAIL_LIBRARY
)

set(LIBMIMEMAIL_INCLUDE_DIRS ${LIBMIMEMAIL_INCLUDE_DIR})
set(LIBMIMEMAIL_LIBRARIES    ${LIBMIMEMAIL_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    Sitter
    REQUIRED_VARS
        LIBMIMEMAIL_INCLUDE_DIR
        LIBMIMEMAIL_LIBRARY
)

# vim: ts=4 sw=4 et

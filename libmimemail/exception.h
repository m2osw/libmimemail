// Copyright (c) 2011-2022  Made to Order Software Corp.  All Rights Reserved
//
// https://snapwebsites.org/project/libmimemail
// contact@m2osw.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
#pragma once

// libexcept
//
#include    <libexcept/exception.h>



namespace libmimemail
{



DECLARE_LOGIC_ERROR(libmimemail_logic_error);

DECLARE_OUT_OF_RANGE(libmimemail_out_of_range);

DECLARE_MAIN_EXCEPTION(libmimemail_exception);

DECLARE_EXCEPTION(libmimemail_exception, invalid_parameter);
DECLARE_EXCEPTION(libmimemail_exception, called_multiple_times);
DECLARE_EXCEPTION(libmimemail_exception, called_after_end_header);
DECLARE_EXCEPTION(libmimemail_exception, missing_parameter);
DECLARE_EXCEPTION(libmimemail_exception, too_many_levels);



} // namespace libmimemail
// vim: ts=4 sw=4 et

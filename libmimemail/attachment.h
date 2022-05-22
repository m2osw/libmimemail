// Copyright (c) 2011-2022  Made to Order Software Corp.  All Rights Reserved.
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
// Snap Websites Server -- snap watchdog library
// Snap Websites Servers -- create a feed where you can write an email
#pragma once

// edhttp
//
#include    <edhttp/quoted_printable.h>


// snapdev
//
#include    <snapdev/brs.h>
#include    <snapdev/case_insensitive_string.h>


// C++
//
#include    <map>



namespace libmimemail
{



typedef std::map<snapdev::case_insensitive_string, std::string> header_map_t;

class attachment
{
public:
    typedef std::vector<attachment>         vector_t;

                            attachment();
    virtual                 ~attachment();

    // data ("matter" of this attachment)
    //
    void                    set_data(std::string const & data, std::string mime_type = std::string());
    void                    quoted_printable_encode_and_set_data(
                                        std::string const & data
                                      , std::string const & mime_type = std::string()
                                      , int flags = edhttp::QUOTED_PRINTABLE_FLAG_LFONLY
                                                  | edhttp::QUOTED_PRINTABLE_FLAG_NO_LONE_PERIOD);
    std::string             get_data() const;

    // header for this header
    //
    void                    set_content_disposition(
                                  std::string const & filename
                                , time_t modification_date = 0
                                , std::string const & attachment_type = "attachment");
    void                    add_header(std::string const & name, std::string const & value);
    void                    remove_header(std::string const & name);
    bool                    has_header(std::string const & name) const;
    std::string             get_header(std::string const & name) const;
    header_map_t const &    get_all_headers() const;

    // sub-attachment (one level available only)
    //
    void                    add_related(attachment const & a);
    int                     get_related_count() const;
    attachment &            get_related(int index) const;

    // "internal" functions used to save/restore the data in/from a buffer
    //
    void                    serialize(snapdev::serializer<std::stringstream> & out) const;
    void                    deserialize(snapdev::deserializer<std::stringstream> & in, bool is_sub_attachment);

    bool                    operator == (attachment const & rhs) const;

private:
    bool                    process_hunk(
                                  snapdev::deserializer<std::stringstream> & in
                                , snapdev::field_t const & field);

    header_map_t            f_headers = header_map_t();
    std::string             f_data = std::string();
    bool                    f_is_sub_attachment = false;
    vector_t                f_sub_attachments = vector_t(); // for HTML data (images, css, ...)

    snapdev::case_insensitive_string
                            f_last_header_name = snapdev::case_insensitive_string();
};



} // namespace libmimemail
// vim: ts=4 sw=4 et

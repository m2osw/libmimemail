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

// self
//
#include    <libmimemail/attachment.h>
#include    <libmimemail/exception.h>


// snapdev
//
#include    <snapdev/case_insensitive_string.h>


// C++
//
#include    <map>
#include    <memory>



namespace libmimemail
{



constexpr int const        EMAIL_MAJOR_VERSION = 1;
constexpr int const        EMAIL_MINOR_VERSION = 1;

enum class priority_t
{
    EMAIL_PRIORITY_BULK = 1,
    EMAIL_PRIORITY_LOW,
    EMAIL_PRIORITY_NORMAL,
    EMAIL_PRIORITY_HIGH,
    EMAIL_PRIORITY_URGENT
};



// create email then use the sendemail() function to send it.
// if you want to save it and send it later, you can serialize/unserialize it too
//
class email
{
public:
    typedef std::map<std::string, std::string>                      parameter_map_t;

                            email();
    virtual                 ~email();

    // basic flags and strings
    //
    void                    set_branding(bool branding = true);
    bool                    get_branding() const;
    void                    set_cumulative(std::string const & object);
    std::string const &     get_cumulative() const;
    void                    set_site_key(std::string const & site_key);
    std::string const &     get_site_key() const;
    void                    set_email_path(std::string const & email_path);
    std::string const &     get_email_path() const;
    void                    set_email_key(std::string const & site_key);
    std::string const &     get_email_key() const;
    time_t                  get_time() const;

    // headers
    //
    void                    set_from(std::string const & from);
    void                    set_to(std::string const & to);
    void                    set_priority(priority_t priority = priority_t::EMAIL_PRIORITY_NORMAL);
    void                    set_subject(std::string const & subject);
    void                    add_header(std::string const & name, std::string const & value);
    void                    remove_header(std::string const & name);
    bool                    has_header(std::string const & name) const;
    std::string             get_header(std::string const & name) const;
    header_map_t const &    get_all_headers() const;

    // attachments
    //
    void                    set_body_attachment(attachment const & data);
    void                    add_attachment(attachment const & data);
    int                     get_attachment_count() const;
    attachment &            get_attachment(int index) const;

    // parameters (like headers but not included in email and names are
    // case sensitive)
    //
    void                    add_parameter(std::string const & name, std::string const & value);
    std::string             get_parameter(std::string const & name) const;
    parameter_map_t const & get_all_parameters() const;

    // functions used to save the data serialized (used by the sendmail plugin)
    //
    void                    serialize(brs::serializer<std::stringstream> & out) const;
    void                    deserialize(brs::deserializer<std::stringstream> & in);

    bool                    send() const;

    bool                    operator == (email const & rhs) const;

private:
    bool                    process_hunk(
                                  brs::deserializer<std::stringstream> & in
                                , brs::field_t const & field);

    bool                    f_branding = true;
    std::string             f_cumulative = std::string();
    std::string             f_site_key = std::string();
    std::string             f_email_path = std::string();
    std::string             f_email_key = std::string(); // set on post_email()
    time_t                  f_time = static_cast<time_t>(-1);
    header_map_t            f_headers = header_map_t();
    attachment::vector_t    f_attachments = attachment::vector_t();
    parameter_map_t         f_parameters = parameter_map_t();
};




} // namespace libmimemail
// vim: ts=4 sw=4 et

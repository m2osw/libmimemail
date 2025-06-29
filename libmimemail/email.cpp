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

// self
//
#include    "libmimemail/email.h"

#include    "libmimemail/names.h"
#include    "libmimemail/version.h"


// edhttp
//
#include    <edhttp/http_date.h>
#include    <edhttp/mime_type.h>
#include    <edhttp/names.h>
#include    <edhttp/quoted_printable.h>
#include    <edhttp/weighted_http_string.h>


// eventdispatcher
//
#include    <cppprocess/io_capture_pipe.h>
#include    <cppprocess/io_data_pipe.h>
#include    <cppprocess/process.h>


// snaplogger
//
#include    <snaplogger/message.h>


// libtld
//
#include    <libtld/tld.h>


// last include
//
#include    <snapdev/poison.h>



namespace libmimemail
{

namespace
{

/** \brief Copy the filename if defined.
 *
 * Check whether the filename is defined in the Content-Disposition
 * or the Content-Type fields and make sure to duplicate it in
 * both fields. This ensures that most email systems have access
 * to the filename.
 *
 * \note
 * The valid location of the filename is the Content-Disposition,
 * but it has been saved in the 'name' sub-field of the Content-Type
 * field and some tools only check that field.
 *
 * \param[in,out] attachment_headers  The headers to be checked for
 *                                    a filename.
 */
void copy_filename_to_content_type(header_map_t & attachment_headers)
{
    if(attachment_headers.find(edhttp::g_name_edhttp_field_content_disposition) != attachment_headers.end()
    && attachment_headers.find(edhttp::g_name_edhttp_field_content_type) != attachment_headers.end())
    {
        // both fields are defined, copy the filename as required
        std::string const content_disposition(attachment_headers[edhttp::g_name_edhttp_field_content_disposition]);
        std::string const content_type(attachment_headers[edhttp::g_name_edhttp_field_content_type]);

        edhttp::weighted_http_string content_disposition_subfields(content_disposition);
        edhttp::weighted_http_string content_type_subfields(content_type);

        edhttp::string_part::vector_t & content_disposition_parts(content_disposition_subfields.get_parts());
        edhttp::string_part::vector_t & content_type_parts(content_type_subfields.get_parts());

        if(content_disposition_parts.size() > 0
        && content_type_parts.size() > 0)
        {
            // we only use part 1 (there should not be more than one though)
            //
            std::string const filename(content_disposition_parts[0].get_parameter("filename"));
            if(!filename.empty())
            {
                // okay, we found the filename in the Content-Disposition,
                // copy that to the Content-Type
                //
                // Note: we always force the name parameter so if it was
                //       already defined, we make sure it is the same as
                //       in the Content-Disposition field
                //
                content_type_parts[0].add_parameter("name", filename);
                attachment_headers[edhttp::g_name_edhttp_field_content_type] = content_type_subfields.to_string();
            }
            else
            {
                std::string const name(content_type_parts[0].get_parameter("name"));
                if(!name.empty())
                {
                    // Somehow the filename is defined in the Content-Type field
                    // but not in the Content-Disposition...
                    //
                    // copy it to the Content-Disposition too (where it should be)
                    //
                    content_disposition_parts[0].add_parameter("filename", name);
                    attachment_headers[edhttp::g_name_edhttp_field_content_disposition] = content_disposition_subfields.to_string();
                }
            }
        }
    }
}


}
// no name namespace










///////////
// EMAIL //
///////////




/** \brief Initialize an email object.
 *
 * This function initializes an email object making it ready to be
 * setup before processing.
 *
 * The function takes no parameter, although a certain number of
 * parameters are required and must be defined before the email
 * can be sent:
 *
 * \li From -- the name/email of the user sending this email.
 * \li To -- the name/email of the user to whom this email is being sent,
 *           there may be multiple recipients and they may be defined
 *           in Cc or Bcc as well as the To list. The To can also be
 *           defined as a list alias name in which case the backend
 *           will send the email to all the subscribers of that list.
 * \li Subject -- the subject must include something.
 * \li Content -- at least one attachment must be added as the body.
 *
 * Attachments support text emails, HTML pages, and any file (image,
 * PDF, etc.). There is no specific limit to the number of attachments
 * or the size per se, although more email systems do limit the size
 * of an email so we do enforce some limit (i.e. 25Mb).
 */
email::email()
    : f_time(time(nullptr))
{
}


/** \brief Clean up the email object.
 *
 * This function ensures that an email object is cleaned up before
 * getting freed.
 */
email::~email()
{
}


/** \brief Change whether the branding is to be shown or not.
 *
 * By default, the send() function includes a couple of branding
 * headers:
 *
 * \li X-Generated-By
 * \li X-Mailer
 *
 * Those two headers can be removed by setting the branding to false.
 *
 * By default the branding is turned on meaning that it will appear
 * in your emails. Obviously, your mail server can later overwrite
 * or remove those fields.
 *
 * \param[in] branding  The new value for the branding flag, true by default.
 */
void email::set_branding(bool branding)
{
    f_branding = branding;
}


/** \brief Retrieve the branding flag value.
 *
 * This function returns true if the branding of the Snap! system will
 * appear in the email.
 *
 * \return true if branding is on, false otherwise.
 */
bool email::get_branding() const
{
    return f_branding;
}


/** \brief Mark this email as being cumulative.
 *
 * A cumulative email is not sent immediately. Instead it is stored
 * and sent at a later time once certain thresholds are reached.
 * There are two thresholds used at this time: a time threshold, a
 * user may want to receive at most one email every few days; and
 * a count threshold, a user may want to receive an email for every
 * X events.
 *
 * Also, our system is capable to cumulate using an overwrite so
 * the receiver gets one email even if the same object was modified
 * multiple times. For example an administrator may want to know
 * when a type of pages gets modified, but he doesn't want to know
 * of each little change (i.e. the editor may change the page 5
 * times in a row as he/she finds things to tweak over and over
 * again.) The name of the \p object passed as a parameter allows
 * the mail system to cumulate using an overwrite and thus mark
 * that this information should really only be sent once (i.e.
 * instead of saying 10 times that page X changed, the mail system
 * can say it once [although we will include how many edits were
 * made as an additional piece of information.])
 *
 * Note that the user may mark all emails that he/she receives as
 * cumulative or non-cumulative so this flag is useful but it can
 * be ignored by the receivers. The priority can be used by the
 * receiver to decide what to do with an email. (i.e. send urgent
 * emails immediately.)
 *
 * \note
 * You may call the set_cumulative() function with an empty string
 * to turn off the cumulative feature for that email.
 *
 * \warning
 * This feature is not yet implemented by the sendmail plugin. Note
 * that is only for that plugin and not for the email class here
 * which has no knowledge of how to cumulate multiple emails into
 * one.
 *
 * \param[in] object  The name of the object being worked on.
 *
 * \sa get_cumulative()
 */
void email::set_cumulative(std::string const & object)
{
    f_cumulative = object;
}


/** \brief Check the cumulative information.
 *
 * This function is used to retreive the cumulative information as saved
 * using the set_cumulative().
 *
 * \warning
 * This feature is not yet implemented by the sendmail plugin. Note
 * that is only for that plugin and not for the email class here
 * which has no knowledge of how to cumulate multiple emails into
 * one.
 *
 * \return A string representing the way the cumulative feature should work.
 *
 * \sa set_cumulative()
 */
std::string const & email::get_cumulative() const
{
    return f_cumulative;
}


/** \brief Set the site key of the site sending this email.
 *
 * The site key is saved in the email whenever the post_email() function
 * is called. You do not have to define it, it will anyway be overwritten.
 *
 * The site key is used to check whether an email is being sent to a group
 * and that group is a mailing list. In that case we've got to have the
 * name of the mailing list defined as "\<site-key>: \<list-name>" thus we
 * need access to the site key that generates the email at the time we
 * manage the email (which is from the backend that has no clue what the
 * site key is when reached).
 *
 * \param[in] site_key  The name (key/URI) of the site being built.
 */
void email::set_site_key(std::string const & site_key)
{
    f_site_key = site_key;
}


/** \brief Retrieve the site key of the site that generated this email.
 *
 * This function retrieves the value set by the set_site_key() function.
 * It returns an empty string until the post_email() function is called
 * because before that it is not set.
 *
 * The main reason for having the site key is to search the list of
 * email lists when the email gets sent to the end user.
 *
 * \return The site key of the site that generated the email.
 */
std::string const & email::get_site_key() const
{
    return f_site_key;
}


/** \brief Define the path to the email in the system.
 *
 * This function sets up the path of the email subject, body, and optional
 * attachments.
 *
 * Other attachments can also be added to the email. However, when a path
 * is defined, the title and body of that page are used as the subject and
 * the body of the email.
 *
 * \note
 * At the time an email gets sent, the permissions of a page are not
 * checked.
 *
 * \warning
 * If you are not in a plugin, this feature and the post will not work for
 * you. Instead you must explicitly define the body and attach it with
 * the set_body_attachment() function. It is not required to add the
 * body attachment first, but it has to be added for the email to work
 * as expected (obviously?!)
 *
 * \param[in] email_path  The path to a page that will be used as the email subject, body, and attachments
 */
void email::set_email_path(std::string const & email_path)
{
    f_email_path = email_path;
}


/** \brief Retrieve the path to the page used to generate the email.
 *
 * This email path is set to a page that represents the subject (title) and
 * body of the email. It may also have attachments linked to it.
 *
 * If the path is empty, then the email is generated using the email object
 * and its attachment, the first attachment being the body of the email.
 *
 * \return The path to the page to be used to generate the email subject and
 *         title.
 */
std::string const & email::get_email_path() const
{
    return f_email_path;
}


/** \brief Set the email key.
 *
 * When a new email is posted, it is assigned a unique number used as a
 * key in different places.
 *
 * \param[in] email_key  The name (key/URI) of the site being built.
 */
void email::set_email_key(std::string const & email_key)
{
    f_email_key = email_key;
}


/** \brief Retrieve the email key.
 *
 * This function retrieves the value set by the set_email_key() function.
 *
 * The email key is set when you call the post_email() function. It is a
 * random number that we also save in the email object so we can keep using
 * it as we go.
 *
 * \return The email key.
 */
std::string const & email::get_email_key() const
{
    return f_email_key;
}


/** \brief Retrieve the time when the email object was created.
 *
 * This function retrieves the time when the email was first created.
 *
 * \return The time when the email object was created.
 */
time_t email::get_time() const
{
    return f_time;
}


/** \brief Save the name and email address of the sender.
 *
 * This function saves the name and address of the sender. It has to
 * be valid according to RFC 2822.
 *
 * If you call this function multiple times, only the last \p from
 * information is kept.
 *
 * \note
 * The set_from() function is the same as calling the add_header() with
 * "From" as the field name and \p from as the value. To retrieve that
 * field, you have to use the get_header() function.
 *
 * \exception sendmail_exception_invalid_argument
 * If the \p from parameter is not a valid email address (as per RCF
 * 2822) or there isn't exactly one email address in that parameter,
 * then this exception is raised.
 *
 * \param[in] from  The name and email address of the sender
 */
void email::set_from(std::string const & from)
{
    // parse the email to verify that it is valid
    //
    tld_email_list emails;
    if(emails.parse(from, 0) != TLD_RESULT_SUCCESS)
    {
        throw invalid_parameter(
                  "email::set_from(): invalid \"From:\" email in \""
                + from
                + "\".");
    }
    if(emails.count() != 1)
    {
        throw invalid_parameter("email::set_from(): multiple \"From:\" emails");
    }

    // save the email as the From email address
    //
    f_headers[g_name_libmimemail_email_from] = from;
}


/** \brief Save the names and email addresses of the receivers.
 *
 * This function saves the names and addresses of the receivers. The list
 * of receivers has to be valid according to RFC 2822.
 *
 * If you are call this function multiple times, only the last \p to
 * information is kept.
 *
 * \note
 * The set_to() function is the same as calling the add_header() with
 * "To" as the field name and \p to as the value. To retrieve that
 * field, you have to use the get_header() function.
 *
 * \warning
 * In most cases you can enter any number of receivers, however, when
 * using the email object directly, it is likely to fail if you do so.
 * The sendmail plugin knows how to handle a list of destinations, though.
 *
 * \exception invalid_parameter
 * If the \p to parameter is not a valid list of email addresses (as per
 * RFC 2822) or there is not at least one email address then this exception
 * is raised.
 *
 * \param[in] to  The list of names and email addresses of the receivers.
 */
void email::set_to(std::string const & to)
{
    // parse the email to verify that it is valid
    //
    tld_email_list emails;
    if(emails.parse(to, 0) != TLD_RESULT_SUCCESS)
    {
        throw invalid_parameter("email::set_to(): invalid \"To:\" email");
    }
    if(emails.count() < 1)
    {
        // this should never happen because the parser will instead return
        // a result other than TLD_RESULT_SUCCESS
        //
        throw invalid_parameter("email::set_to(): not even one \"To:\" email");
    }

    // save the email as the To email address
    //
    f_headers[g_name_libmimemail_email_to] = to;
}


/** \brief The priority is a somewhat arbitrary value defining the email urgency.
 *
 * Many mail system define a priority but it really isn't defined in the
 * RFC 2822 so the value is not well defined.
 *
 * The priority is saved in the X-Priority header.
 *
 * \param[in] priority  The priority of this email.
 */
void email::set_priority(priority_t priority)
{
    std::string name;
    switch(priority)
    {
    case priority_t::EMAIL_PRIORITY_BULK:
        name = g_name_libmimemail_email_priority_bulk;
        break;

    case priority_t::EMAIL_PRIORITY_LOW:
        name = g_name_libmimemail_email_priority_low;
        break;

    case priority_t::EMAIL_PRIORITY_NORMAL:
        name = g_name_libmimemail_email_priority_normal;
        break;

    case priority_t::EMAIL_PRIORITY_HIGH:
        name = g_name_libmimemail_email_priority_high;
        break;

    case priority_t::EMAIL_PRIORITY_URGENT:
        name = g_name_libmimemail_email_priority_urgent;
        break;

    default:
        throw invalid_parameter(
                  "email::set_priority(): Unknown priority \""
                + std::to_string(static_cast<int>(priority))
                + "\".");

    }

    f_headers[g_name_libmimemail_email_x_priority] = std::to_string(static_cast<int>(priority)) + " (" + name + ")";
    f_headers[g_name_libmimemail_email_x_msmail_priority] = name;
    f_headers[g_name_libmimemail_email_importance] = name;
    f_headers[g_name_libmimemail_email_precedence] = name;
}


/** \brief Set the email subject.
 *
 * This function sets the subject of the email. Anything is permitted although
 * you should not send emails with an empty subject.
 *
 * The system takes care of encoding the subject if required. It will also trim
 * it and remove any unwanted characters (tabs, new lines, etc.)
 *
 * The subject line is also silently truncated to a reasonable size.
 *
 * Note that if the email is setup with a path to a page, the title of that
 * page is used as the default subject. If the set_subject() function is
 * called with a valid subject (not empty) then the page title is ignored.
 *
 * \note
 * The set_subject() function is the same as calling the add_header() with
 * "Subject" as the field name and \p subject as the value.
 *
 * \param[in] subject  The subject of the email.
 */
void email::set_subject(std::string const & subject)
{
    f_headers[g_name_libmimemail_email_subject] = subject;
}


/** \brief Add a header to the email.
 *
 * The system takes care of most of the email headers but this function gives
 * you the possibility to add more.
 *
 * Note that the priority should instead be set with the set_priority()
 * function. This way it is more likely to work in all system that the
 * sendmail plugin supports.
 *
 * The content type should not be set. The system automatically takes care of
 * that for you including required encoding information, attachments, etc.
 *
 * The To, Cc, and Bcc fields are defined in this way. If multiple
 * destinations are defined, you must concatenate them in the
 * \p value parameter before calling this function.
 *
 * Note that the name of a header is case insensitive. So the names
 * "Content-Type" and "content-type" represent the same header. Which
 * one will be used when generating the output is a non-disclosed internal
 * functionality. You probably want to use the SNAP_SENDMAIL_HEADER_...
 * names anyway (at least for those that are defined.)
 *
 * \warning
 * Also the function is called 'add', because you may add as many headers as you
 * need, the function does NOT cumulate data within one field. Instead it
 * overwrites the content of the field. This is one way to replace an unwanted
 * value or force the content of a field for a given email.
 *
 * \exception sendmail_exception_invalid_argument
 * The name of a header cannot be empty. This exception is raised if
 * \p name is empty. The field name is also validated by the TLD library
 * and must be composed of letters, digits, the dash character, and it
 * has to start with a letter. The case is not important, however.
 * Also, if the field represents an email or a list of emails, the
 * value is also checked for validity.
 *
 * \param[in] name  A valid header name.
 * \param[in] value  The value of this header.
 */
void email::add_header(std::string const & name, std::string const & value)
{
    // first define a type
    //
    tld_email_field_type type(tld_email_list::email_field_type(name));
    if(type == TLD_EMAIL_FIELD_TYPE_INVALID)
    {
        // this includes the case where the field name is empty
        throw invalid_parameter("email::add_header(): Invalid header name for a header name.");
    }

    // if type is not unknown, check the actual emails
    //
    // "UNKNOWN" means we don't consider the value of this header to be
    // one or more emails
    //
    if(type != TLD_EMAIL_FIELD_TYPE_UNKNOWN)
    {
        // The Bcc and alike fields may be empty
        //
        if(type != TLD_EMAIL_FIELD_TYPE_ADDRESS_LIST_OPT
        || !value.empty())
        {
            // if not unknown then we should check the field value
            // as a list of emails
            //
            tld_email_list emails;
            if(emails.parse(value, 0) != TLD_RESULT_SUCCESS)
            {
                // TODO: this can happen if a TLD becomes obsolete and
                //       a user did not update one's email address.
                //
                throw invalid_parameter(
                          "email::add_header(): Invalid emails in header field: \""
                        + name
                        + ": "
                        + value
                        + "\"");
            }

            // for many fields it can have at most one mailbox
            //
            if(type == TLD_EMAIL_FIELD_TYPE_MAILBOX
            && emails.count() != 1)
            {
                throw invalid_parameter(
                          "email::add_header(): Header field expects exactly one email in: \""
                        + name
                        + ": "
                        + value
                        + "\"");
            }
        }
    }

    f_headers[snapdev::to_case_insensitive_string(name)] = value;
}


/** \brief Remove a header.
 *
 * This function searches for the \p name header and removes it from the
 * list of defined headers. This is different from setting the value of
 * a header to the empty string as the header continues to exist.
 *
 * In most cases, you may just set a header to the empty string
 * to delete it, however, removing it is cleaner.
 *
 * \param[in] name  The name of the header to get rid of.
 */
void email::remove_header(std::string const & name)
{
    auto const it(f_headers.find(snapdev::to_case_insensitive_string(name)));
    if(it != f_headers.end())
    {
        f_headers.erase(it);
    }
}


/** \brief Check whether a header is defined or not.
 *
 * This function returns true if the header was defined (add_header() was
 * called at least once on that header name.)
 *
 * This function will return true even if the header was set to the empty
 * string.
 *
 * \param[in] name  The name of the header to get rid of.
 */
bool email::has_header(std::string const & name) const
{
    if(name.empty())
    {
        throw invalid_parameter("email::has_header(): Cannot check for a header with an empty name.");
    }

    return f_headers.find(snapdev::to_case_insensitive_string(name)) != f_headers.end();
}


/** \brief Retrieve the value of a header.
 *
 * This function returns the value of the named header. If the header
 * is not currently defined, this function returns an empty string.
 *
 * To know whether a header is defined, you may instead call the
 * has_header() even if in most cases an empty string is very much
 * similar to an undefined header.
 *
 * \exception sendmail_exception_invalid_argument
 * The name of a header cannot be empty. This exception is raised if
 * \p name is empty.
 *
 * \param[in] name  A valid header name.
 *
 * \return The current value of that header or an empty string if undefined.
 */
std::string email::get_header(std::string const & name) const
{
    if(name.empty())
    {
        throw invalid_parameter("email::get_header(): Cannot retrieve a header with an empty name.");
    }

    auto const it(f_headers.find(snapdev::to_case_insensitive_string(name)));
    if(it != f_headers.end())
    {
        return it->second;
    }

    // return f_headers[name] -- this would create an entry for f_headers[name] for nothing
    return std::string();
}


/** \brief Get all the headers defined in this email.
 *
 * This function returns the map of the headers defined in this email. This
 * can be used to quickly scan all the headers.
 *
 * \note
 * It is important to remember that since this function returns a reference
 * to the map of headers, it may break if you call add_header() while going
 * through the references unless you make a copy.
 *
 * \return A direct constant reference to the internal header map.
 */
header_map_t const & email::get_all_headers() const
{
    return f_headers;
}


/** \brief Add the body attachment to this email.
 *
 * When creating an email with a path to a page (which is close to mandatory
 * if you want to have translation and let users of your system to be able
 * to edit the email in all languages.)
 *
 * This function should be private because it should only be used internally.
 * Unfortunately, the function is used from the outside. But you've been
 * warn. Really, this is using a push_front() instead of a push_back() it
 * is otherwise the same as the add_attachment() function and you may want
 * to read that function's documentation too.
 *
 * \param[in] data  The attachment to add as the body of this email.
 *
 * \sa email::add_attachment()
 */
void email::set_body_attachment(attachment const & data)
{
    f_attachments.insert(f_attachments.begin(), data);
}


/** \brief Add an attachment to this email.
 *
 * All data appearing in the body of the email is defined using attachments.
 * This includes the normal plain text body if you use one. See the
 * attachment class for details on how to create an attachment
 * for an email.
 *
 * Note that if you want to add a plain text and an HTML version to your
 * email, these are sub-attachments to one attachment of the email defined
 * as alternatives. If only that one attachment is added to an email then
 * it won't be made a sub-attachment in the final email buffer.
 *
 * \b IMPORTANT \b NOTE: the body and subject of emails are most often defined
 * using a path to a page. This means the first attachment is to be viewed
 * as an attachment, not the main body. Also, the attachments of the page
 * are also viewed as attachments of the email and will appear before the
 * attachments added here.
 *
 * \note
 * It is important to note that the attachments are written in the email
 * in the order they are defined here. It is quite customary to add the
 * plain text first, then the HTML version, then the different files to
 * attach to the email.
 *
 * \param[in] data  The email attachment to add by copy.
 *
 * \sa email::set_body_attachment()
 */
void email::add_attachment(attachment const & data)
{
    f_attachments.push_back(data);
}


/** \brief Retrieve the number of attachments defined in this email.
 *
 * This function defines the number of attachments that were added to this
 * email. This is useful to retrieve the attachments with the
 * get_attachment() function.
 *
 * \return The number of attachments defined in this email.
 *
 * \sa add_attachment()
 * \sa get_attachment()
 */
int email::get_attachment_count() const
{
    return f_attachments.size();
}


/** \brief Retrieve the specified attachement.
 *
 * This function gives you a read/write reference to the specified
 * attachment. This is used by plugins that need to access email
 * data to filter it one way or the other (i.e. change all the tags
 * with their corresponding values.)
 *
 * The \p index parameter must be a number between 0 and
 * get_attachment_count() minus one. If no attachments were added
 * then this function cannot be called.
 *
 * \exception out_of_range
 * If the index is out of range, this exception is raised.
 *
 * \param[in] index  The index of the attachment to retrieve.
 *
 * \return A reference to the corresponding attachment.
 *
 * \sa add_attachment()
 * \sa get_attachment_count()
 */
attachment & email::get_attachment(int index) const
{
    if(static_cast<size_t>(index) >= f_attachments.size())
    {
        throw std::out_of_range("email::get_attachment() called with an invalid index");
    }
    return const_cast<attachment &>(f_attachments[index]);
}


/** \brief Add a parameter to the email.
 *
 * Whenever you create an email, you may be able to offer additional
 * parameters that are to be used as token replacement in the email.
 * For example, when creating a new user, we ask the user to verify his
 * email address. This is done by creating a session identifier and then
 * asking the user to go to the special page /verify/\<session>. That
 * way we know that the user received the email (although it may not
 * exactly be the right person...)
 *
 * The name of the parameter should be something like "users::verify",
 * i.e. it should be namespace specific to not clash with sendmail or
 * other plugins parameters.
 *
 * All parameters have case sensitive names. So sendmail and Sendmail
 * are not equal. However, all parameters should use lowercase only
 * to match conventional XML tag and attribute names.
 *
 * \warning
 * Also the function is called 'add', because you may add as many parameters
 * as you have available, the function does NOT cumulate data within one field.
 * Instead it overwrites the content of the field if set more than once. This
 * is one way to replace an unwanted value or force the content of a field
 * for a given email.
 *
 * \exception invalid_parameter
 * The name of a parameter cannot be empty. This exception is raised if
 * \p name is empty.
 *
 * \param[in] name  A valid parameter name.
 * \param[in] value  The value of this header.
 */
void email::add_parameter(std::string const & name, std::string const & value)
{
    if(name.empty())
    {
        throw invalid_parameter("email::add_parameter(): Cannot add a parameter with an empty name.");
    }

    f_parameters[name] = value;
}


/** \brief Retrieve the value of a named parameter.
 *
 * This function returns the value of the named parameter. If the parameter
 * is not currently defined, this function returns an empty string.
 *
 * \exception invalid_parameter
 * The name of a parameter cannot be empty. This exception is raised if
 * \p name is empty.
 *
 * \param[in] name  A valid parameter name.
 *
 * \return The current value of that parameter or an empty string if undefined.
 */
std::string email::get_parameter(std::string const & name) const
{
    if(name.empty())
    {
        throw invalid_parameter("email::get_parameter(): Cannot retrieve a parameter with an empty name.");
    }
    auto const it(f_parameters.find(name));
    if(it != f_parameters.end())
    {
        return it->second;
    }

    return std::string();
}


/** \brief Get all the parameters defined in this email.
 *
 * This function returns the map of the parameters defined in this email.
 * This can be used to quickly scan all the parameters.
 *
 * \note
 * It is important to remember that since this function returns a reference
 * to the map of parameters, it may break if you call add_parameter() while
 * going through the references.
 *
 * \return A direct reference to the internal parameter map.
 */
const email::parameter_map_t & email::get_all_parameters() const
{
    return f_parameters;
}


/** \brief Unserialize an email message.
 *
 * This function unserializes an email message that was serialized using
 * the serialize() function.
 *
 * You are expected to first create an email object and then call this
 * function with the data parameter set as the string that the serialize()
 * function returned.
 *
 * You may setup some default headers such as the X-Mailer value in your
 * email object before calling this function. If such header information
 * is defined in the serialized data then it will be overwritten with
 * that data. Otherwise it will remain the same.
 *
 * The function doesn't return anything. Instead it unserializes the
 * \p data directly in this email object.
 *
 * \param[in] data  The serialized email data to transform.
 *
 * \sa serialize()
 */
void email::deserialize(snapdev::deserializer<std::stringstream> & in)
{
    snapdev::deserializer<std::stringstream>::process_hunk_t func(std::bind(
                  &email::process_hunk
                , this
                , std::placeholders::_1
                , std::placeholders::_2));
    if(!in.deserialize(func))
    {
        SNAP_LOG_WARNING
            << "email unserialization stopped early."
            << SNAP_LOG_SEND;
    }
}


/** \brief Read the contents of one tag from the reader.
 *
 * This function reads the contents of the main email tag. It calls
 * the attachment deserialize() as required whenever an attachment
 * is found in the stream.
 *
 * \param[in] name  The name of the tag being read.
 * \param[in] r  The reader used to read the input data.
 */
bool email::process_hunk(
      snapdev::deserializer<std::stringstream> & in
    , snapdev::field_t const & field)
{
    switch(field.f_name[0])
    {
    case 'a':
        if(field.f_name == "attachment")
        {
            attachment a;
            a.deserialize(in, false);
            add_attachment(a);
        }
        break;

    case 'b':
        if(field.f_name == "branding")
        {
            in.read_data(f_branding);
        }
        break;

    case 'c':
        if(field.f_name == "cumulative")
        {
            in.read_data(f_cumulative);
        }
        break;

    case 'e':
        if(field.f_name == "email_path")
        {
            in.read_data(f_email_path);
        }
        else if(field.f_name == "email_key")
        {
            in.read_data(f_email_key);
        }
        break;

    case 'h':
        if(field.f_name == "header")
        {
            std::string value;
            in.read_data(value);
            f_headers[snapdev::to_case_insensitive_string(field.f_sub_name)] = value;
        }
        break;

    case 'p':
        if(field.f_name == "parameter")
        {
            std::string value;
            in.read_data(value);
            f_headers[snapdev::to_case_insensitive_string(field.f_sub_name)] = value;
        }
        break;

    case 's':
        if(field.f_name == "site_key")
        {
            in.read_data(f_site_key);
        }
        break;

    }

    return true;
}


/** \brief Transform the email in one string.
 *
 * This function transform the email data in one string so it can easily
 * be saved in the Cassandra database. This is done so it can be sent to
 * the recipients using the backend process preferably on a separate
 * computer (i.e. a computer that is not being accessed by your web
 * clients.)
 *
 * The deserialize() function can be used to restore an email that was
 * previously serialized with this function.
 *
 * \param[out] out  The serializer were the email is to be written.
 *
 * \sa deserialize()
 */
void email::serialize(snapdev::serializer<std::stringstream> & out) const
{
    std::string const version(
                  std::to_string(EMAIL_MAJOR_VERSION)
                + '.'
                + std::to_string(EMAIL_MINOR_VERSION));
    out.add_value("version", version);

    out.add_value("branding", f_branding);

    out.add_value_if_not_empty("cumulative", f_cumulative);

    out.add_value("site_key", f_site_key);
    out.add_value("email_path", f_email_path);
    out.add_value("email_key", f_email_key);
    // TBD: should we save f_time?

    for(auto const & it : f_headers)
    {
        out.add_value("header", snapdev::to_string(it.first), it.second);
    }

    for(auto const & it : f_attachments)
    {
        snapdev::recursive sub_field(out, "attachment");
        it.serialize(out);
    }

    for(auto const & it : f_parameters)
    {
        out.add_value("parameter", it.first, it.second);
    }
}


/** \brief Send this email.
 *
 * This function sends  the specified email. It generates all the body
 * and attachments, etc.
 *
 * Note that the function uses callbacks in order to retrieve the body
 * and attachment from the database as the Snap! environment uses those
 * for much of the data to be sent in emails. However, it is not a
 * requirements in case you want to send an email from another server
 * than a snap_child or snap_backend.
 *
 * \exception email_exception_missing_parameter
 * If the From header or the destination email only are missing this
 * exception is raised.
 *
 * \return true if the send() command worked (note that does not mean the
 * email made it; we'll know later whether it failed if we received a
 * bounced email).
 */
bool email::send() const
{
    // verify that the `From` and `To` headers are defined
    //
    std::string const from(get_header(g_name_libmimemail_email_from));
    std::string const to(get_header(g_name_libmimemail_email_to));

    if(from.empty()
    || to.empty())
    {
        throw missing_parameter("email::send() called without a From or a To header field defined. Make sure you call the set_from() and set_header() functions appropriately.");
    }

    // verify that we have at least one attachment
    // (the body is an attachment)
    //
    int const max_attachments(get_attachment_count());
    if(max_attachments < 1)
    {
        throw missing_parameter("email::send() called without at least one attachment (body).");
    }

    // we want to transform the body from HTML to text ahead of time
    //
    attachment const & body_attachment(get_attachment(0));

    // TODO: verify that the body is indeed HTML!
    //       although html2text works against plain text but that is a waste
    //
    //       also, we should offer a way for the person creating an email
    //       to specify both: a plain text body and an HTML body
    //
    std::string plain_text;
    std::string const body_mime_type(body_attachment.get_header(edhttp::g_name_edhttp_field_content_type));

    // TODO: this test is wrong as it would match things like "text/html-special"
    //
    if(body_mime_type.substr(0, 9) == "text/html")
    {
        cppprocess::process h2t("html2text");
        h2t.set_command("html2text");
        //h2t.add_argument("-help");
        h2t.add_argument("-nobs");
        h2t.add_argument("-utf8");
        h2t.add_argument("-style");
        h2t.add_argument("pretty");
        h2t.add_argument("-width");
        h2t.add_argument("70");
        std::string html_data;

        cppprocess::io_data_pipe::pointer_t in(std::make_shared<cppprocess::io_data_pipe>());
        h2t.set_input_io(in);

        cppprocess::io_capture_pipe::pointer_t out(std::make_shared<cppprocess::io_capture_pipe>());
        h2t.set_output_io(out);

        std::string data(body_attachment.get_data());

        // TODO: support other encoding, err if not supported
        //
        if(body_attachment.get_header(edhttp::g_name_edhttp_field_content_transfer_encoding)
                                == edhttp::g_name_edhttp_param_quoted_printable)
        {
            // if it was quoted-printable encoded, we have to decode
            //
            // I know, we encode in this very function and could just
            // keep a copy of the original, HOWEVER, the end user could
            // build the whole email with this encoding already in place
            // and thus we anyway would have to decode... This being said,
            // we could have that as an optimization XXX
            //
            html_data = edhttp::quoted_printable_decode(data.data());
        }
        else
        {
            html_data = data.data();
        }
        in->add_input(html_data);

        // conver that HTML to plain text
        //
        int r(h2t.start());
        if(r == 0)
        {
            r = h2t.wait();
        }
        if(r == 0)
        {
            plain_text = out->get_output();
        }
        else
        {
            // no plain text, but let us know that something went wrong at least
            //
            SNAP_LOG_WARNING
                << "An error occurred while executing html2text (exit code: "
                << r
                << ")"
                << SNAP_LOG_SEND;
        }
    }

    // convert the "from" email address in a TLD email address so we can use
    // the f_email_only version for the command line "sender" parameter
    //
    tld_email_list from_list;
    if(from_list.parse(from, 0) != TLD_RESULT_SUCCESS)
    {
        throw invalid_parameter(
                  "email::send() called with invalid sender email address: \""
                + from
                + "\" (parsing failed).");
    }
    tld_email_list::tld_email_t s;
    if(!from_list.next(s))
    {
        throw invalid_parameter(
                  "email::send() called with invalid sender email address: \""
                + from
                + "\" (no email returned).");
    }

    // convert the "to" email address in a TLD email address so we can use
    // the f_email_only version for the command line "to" parameter
    //
    tld_email_list to_list;
    if(to_list.parse(to, 0) != TLD_RESULT_SUCCESS)
    {
        throw invalid_parameter(
                  "email::send() called with invalid destination email address: \""
                + to
                + "\" (parsing failed).");
    }
    tld_email_list::tld_email_t m;
    if(!to_list.next(m))
    {
        throw invalid_parameter(
                  "email::send() called with invalid destination email address: \""
                + to
                + "\" (no email returned).");
    }

    // create an output stream to send the email
    //
    cppprocess::process p("sendmail");
    p.set_command("sendmail");
    p.add_argument("-f");
    p.add_argument(s.f_email_only);
    p.add_argument(m.f_email_only);
    SNAP_LOG_TRACE
        << "sendmail command: ["
        << p.get_command_line()
        << "]"
        << SNAP_LOG_SEND;

    cppprocess::io_data_pipe::pointer_t in(std::make_shared<cppprocess::io_data_pipe>());
    p.set_input_io(in);

    int const start_status(p.start());
    if(start_status != 0)
    {
        SNAP_LOG_ERROR
            << "could not start process \""
            << p.get_name()
            << "\" (command line: "
            << p.get_command_line()
            << ")."
            << SNAP_LOG_SEND;
        return false;
    }

    //snap_pipe spipe(cmd, snap_pipe::mode_t::PIPE_MODE_IN);
    //std::ostream f(&spipe);

    // convert email data to text and send that to the sendmail command line
    //
    header_map_t headers(f_headers);
    bool const body_only(max_attachments == 1 && plain_text.empty());
    std::string boundary;
    if(body_only)
    {
        // if the body is by itself, then its encoding needs to be transported
        // to the main set of headers
        //
        if(body_attachment.get_header(edhttp::g_name_edhttp_field_content_transfer_encoding)
                                == edhttp::g_name_edhttp_param_quoted_printable)
        {
            headers[edhttp::g_name_edhttp_field_content_transfer_encoding]
                                = edhttp::g_name_edhttp_param_quoted_printable;
        }
    }
    else
    {
        // boundary      := 0*69<bchars> bcharsnospace
        // bchars        := bcharsnospace / " "
        // bcharsnospace := DIGIT / ALPHA / "'" / "(" / ")" /
        //                  "+" / "_" / "," / "-" / "." /
        //                  "/" / ":" / "=" / "?"
        //
        // Note: we generate boundaries without special characters
        //       (and especially no spaces or dashes) to make it simpler
        //
        // Note: the boundary starts wity "=S" which is not a valid
        //       quoted-printable sequence of characters (on purpose)
        //
        char const allowed[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"; //'()+_,./:=?";
        boundary.reserve(15 + 20);
        boundary = "=Snap.Websites=";
        for(int i(0); i < 20; ++i)
        {
            // this is just for boundaries, so rand() is more than enough
            // it just needs to not match anything in the emails
            //
            int const c(static_cast<int>(rand() % (sizeof(allowed) - 1)));
            boundary += allowed[c];
        }
        headers[edhttp::g_name_edhttp_field_content_type] = "multipart/mixed;\n  boundary=\"" + boundary + "\"";
        headers[g_name_libmimemail_email_mime_version] = "1.0";
    }

    // setup the "Date: ..." field if not already defined
    //
    if(headers.find(g_name_libmimemail_email_date) == headers.end())
    {
        // the date must be specified in English only which prevents us from
        // using the strftime()
        //
        headers[g_name_libmimemail_email_date] = edhttp::date_to_string(time(nullptr), edhttp::date_format_t::DATE_FORMAT_EMAIL);
    }

    // setup a default "Content-Language: ..." because in general
    // that makes things work better
    //
    if(headers.find(edhttp::g_name_edhttp_field_content_language) == headers.end())
    {
        headers[edhttp::g_name_edhttp_field_content_language] = "en-us";
    }

    for(auto const & it : headers)
    {
        // TODO: the it.second needs to be URI encoded to be valid
        //       in an email; if some characters appear that need
        //       encoding, we should err (we probably want to
        //       capture those in the add_header() though)
        //
        //f << it.first << ": " << it.second << std::endl;
        in->add_input(snapdev::to_string(it.first));
        in->add_input(": ");
        in->add_input(it.second);
        in->add_input("\n");
    }

    // XXX: allow administrators to change the `branding` flag
    //
    if(f_branding)
    {
        //f << "X-Generated-By: Snap! Websites C++ v" SNAPWEBSITES_VERSION_STRING " (https://snapwebsites.org/)" << std::endl
        //  << "X-Mailer: Snap! Websites C++ v" SNAPWEBSITES_VERSION_STRING " (https://snapwebsites.org/)" << std::endl;
        in->add_input(
            "X-Generated-By: Snap! Websites C++ v" LIBMIMEMAIL_VERSION_STRING " (https://snapwebsites.org/)\n"
            "X-Mailer: Snap! Websites C++ v" LIBMIMEMAIL_VERSION_STRING " (https://snapwebsites.org/)\n");
    }

    // end the headers
    //
    //f << std::endl;
    in->add_input("\n");

    if(body_only)
    {
        // in this case we only have one entry, probably HTML, and thus we
        // can avoid the multi-part headers and attachments
        //
        //f << body_attachment.get_data().data() << std::endl;
        in->add_input(body_attachment.get_data().data());
        in->add_input("\n");
    }
    else
    {
        // TBD: should we make this text changeable by client?
        //
        //f << "The following are various parts of a multipart email." << std::endl
        //  << "It is likely to include a text version (first part) that you should" << std::endl
        //  << "be able to read as is." << std::endl
        //  << "It may be followed by HTML and then various attachments." << std::endl
        //  << "Please consider installing a MIME capable client to read this email." << std::endl
        //  << std::endl;
        in->add_input(
                "The following are various parts of a multipart email.\n"
                "It is likely to include a text version (first part) that you should\n"
                "be able to read as is.\n"
                "It may be followed by HTML and then various attachments.\n"
                "Please consider installing a MIME capable client to read this email.\n"
                "\n");

        int i(0);
        if(!plain_text.empty())
        {
            // if we have plain text then we have alternatives
            //
            //f << "--" << boundary << std::endl
            //  << "Content-Type: multipart/alternative;" << std::endl
            //  << "  boundary=\"" << boundary << ".msg\"" << std::endl
            //  << std::endl
            //  << "--" << boundary << ".msg" << std::endl
            //  << "Content-Type: text/plain; charset=\"utf-8\"" << std::endl
            //  //<< "MIME-Version: 1.0" << std::endl -- only show this one in the main header
            //  << "Content-Transfer-Encoding: quoted-printable" << std::endl
            //  << "Content-Description: Mail message body" << std::endl
            //  << std::endl
            //  << quoted_printable::encode(plain_text, quoted_printable::QUOTED_PRINTABLE_FLAG_NO_LONE_PERIOD) << std::endl;
            in->add_input("--");
            in->add_input(boundary);
            in->add_input("\n");
            in->add_input(edhttp::g_name_edhttp_field_content_type);
            in->add_input(": ");
            in->add_input(edhttp::g_name_edhttp_param_multipart_alternative);
            in->add_input(";\n"
                          "  boundary-\"");
            in->add_input(boundary);
            in->add_input(".msg\"\n"
                          "\n");
            in->add_input("--");
            in->add_input(boundary);
            in->add_input(".msg\n");
            in->add_input("\n");
            in->add_input(edhttp::g_name_edhttp_field_content_type);
            in->add_input(": text/plain; charset=\"utf-8\"\n");
            in->add_input(edhttp::g_name_edhttp_field_content_transfer_encoding);
            in->add_input(": ");
            in->add_input(edhttp::g_name_edhttp_param_quoted_printable);
            in->add_input("\n");
            in->add_input(edhttp::g_name_edhttp_field_content_description);
            in->add_input(": Mail message body\n");
            in->add_input("\n");
            in->add_input(edhttp::quoted_printable_encode(
                              plain_text
                            , edhttp::QUOTED_PRINTABLE_FLAG_NO_LONE_PERIOD));
            in->add_input("\n");

            // at this time, this if() should always be true
            //
            if(i < max_attachments)
            {
                // now include the HTML
                //
                //f << "--" << boundary << ".msg" << std::endl;
                in->add_input("--");
                in->add_input(boundary);
                in->add_input(".msg\n");
                for(auto const & it : body_attachment.get_all_headers())
                {
                    //f << it.first << ": " << it.second << std::endl;
                    in->add_input(snapdev::to_string(it.first));
                    in->add_input(": ");
                    in->add_input(it.second);
                    in->add_input("\n");
                }
                // one empty line to end the headers
                //
                in->add_input("\n");

                // here the data in body_attachment is already encoded
                //
                //f << std::endl
                //  << body_attachment.get_data().data() << std::endl
                //  << "--" << boundary << ".msg--" << std::endl
                //  << std::endl;
                in->add_input(body_attachment.get_data().data());
                in->add_input("--");
                in->add_input(boundary);
                in->add_input(".msg--\n\n");

                // we used "attachment" 0, so print the others starting at 1
                //
                i = 1;
            }
        }

        // send the remaining attachments (possibly attachment 0 if
        // we did not have plain text)
        //
        for(; i < max_attachments; ++i)
        {
            // work on this attachment
            //
            attachment const & a(get_attachment(i));

            // send the boundary
            //
            //f << "--" << boundary << std::endl;
            in->add_input("--");
            in->add_input(boundary);
            in->add_input("\n");

            // send  the headers for that attachment
            //
            // we get a copy and modify it slightly by making sure that
            // the filename is defined in both the Content-Disposition
            // and the Content-Type
            //
            header_map_t attachment_headers(a.get_all_headers());
            copy_filename_to_content_type(attachment_headers);
            for(auto const & it : attachment_headers)
            {
                //f << it.first << ": " << it.second << std::endl;
                in->add_input(snapdev::to_string(it.first));
                in->add_input(": ");
                in->add_input(it.second);
                in->add_input("\n");
            }
            // one empty line to end the headers
            //
            //f << std::endl;
            in->add_input("\n");

            // here the data is already encoded
            //
            //f << a.get_data().data() << std::endl;
            in->add_input(a.get_data().data());
            in->add_input("\n");
        }

        // last boundary to end them all
        //
        //f << "--" << boundary << "--" << std::endl;
        in->add_input("--");
        in->add_input(boundary);
        in->add_input("--\n");
    }

    // end the message
    //
    //f << std::endl
    //  << "." << std::endl;
    in->add_input("\n"
                  ".\n");

    // make sure the ostream gets flushed or some data could be left in
    // a cache and never written to the pipe (unlikely since we do not
    // use the cache, but future C++ versions could have a problem.)
    //
    //f.flush();

    // close pipe as soon as we are done writing to it
    // and return true if it all worked as expected
    //
    //return spipe.close_pipe() == 0;

    // TODO: this needs to be using ed::communicator so we need a callback
    //       if we want to support a similar "interface" (i.e. if the caller
    //       wants to know whether it worked)
    //
    return p.wait() == 0;
}


/** \brief Compare two email obejcts for equality.
 *
 * This function checks whether two email objects are equal.
 *
 * \param[in] rhs  The right hand side email.
 *
 * \return true if both emails are considered equal.
 */
bool email::operator == (email const & rhs) const
{
    return f_branding    == rhs.f_branding
        && f_cumulative  == rhs.f_cumulative
        && f_site_key    == rhs.f_site_key
        && f_email_path  == rhs.f_email_path
        && f_email_key   == rhs.f_email_key
        //&& f_time        == rhs.f_time -- this is pretty much never going to be equal so do not compare
        && f_headers     == rhs.f_headers
        && f_attachments == rhs.f_attachments
        && f_parameters  == rhs.f_parameters;
}


}
// namespace libmimemail
// vim: ts=4 sw=4 et

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

// self
//
#include    "libmimemail/attachment.h"

#include    "libmimemail/exception.h"


// edhttp
//
#include    <edhttp/http_date.h>
#include    <edhttp/mime_type.h>
#include    <edhttp/names.h>
#include    <edhttp/uri.h>


// snapdev
//
#include    <snapdev/not_used.h>
#include    <snapdev/pathinfo.h>


// snaplogger
//
#include    <snaplogger/message.h>


// last include
//
#include    <snapdev/poison.h>



namespace libmimemail
{



//////////////////////
// EMAIL ATTACHMENT //
//////////////////////


/** \brief Initialize an email attachment object.
 *
 * You can create an email attachment object, initializes it, and then
 * add it to an email object. The number of attachments is not limited
 * although you should remember that most mail servers limit the total
 * size of an email. It may be 5, 10 or 20Mb, but if you go over, the
 * email will fail.
 */
attachment::attachment()
{
}


/** \brief Clean up an email attachment.
 *
 * This function is here primarily to have a clean virtual table.
 */
attachment::~attachment()
{
}


/** \brief The content of the binary file to attach to this email.
 *
 * This function is used to attach one binary file to the email.
 *
 * If you know the MIME type of the data, it is smart to define it when
 * calling this function so that way you avoid asking the magic library
 * for it. This will save time as the magic library is much slower and
 * if you are positive about the type, it will be correct whereas the
 * magic library could return an invalid value.
 *
 * Also, if this is a file attachment, make sure to add a
 * `Content-Disposition` header to define the filename and
 * modification date as in:
 *
 * \code
 *   Content-Disposition: attachment; filename=my-attachment.pdf;
 *     modification-date="Tue, 29 Sep 2015 16:12:15 -0800";
 * \endcode
 *
 * See the set_content_disposition() function to easily add this
 * field.
 *
 * \note
 * The mime_type can be set to the empty string (`QString()`) to let
 * the system generate the MIME type automatically using the
 * get_mime_type() function.
 *
 * \param[in] data  The data to attach to this email.
 * \param[in] mime_type  The MIME type of the data if known,
 *                       otherwise leave empty.
 *
 * \sa add_header()
 * \sa get_mime_type()
 * \sa set_content_disposition()
 */
void attachment::set_data(std::string const & data, std::string mime_type)
{
    f_data = data;

    // if user did not define the MIME type then ask the magic library
    if(mime_type.empty())
    {
        mime_type = edhttp::get_mime_type(f_data);
    }
    f_headers[edhttp::g_name_edhttp_field_content_type] = mime_type;
}


/** \brief Set the email attachment using quoted printable encoding.
 *
 * In most cases, when you attach something else than just text, you want
 * to encode the data. Even text, if you do not control the length of each
 * line properly, it is likely to get cut at some random length and could
 * end up looking wrong.
 *
 * This function encodes the data using the quoted_printable::encode()
 * function and marks the data encoded in such a way.
 *
 * By default, all you have to do is pass a QByteArray and the rest works
 * on its own, although it is usually a good idea to specify the MIME type
 * if you knowit.
 *
 * The flags parameter can be used to tweak the encoding functionality.
 * The default works with most data, although it does not include the
 * binary flag. See the quoted_printable::encode() function for additional
 * information about these flags.
 *
 * \param[in] data  The data of this a attachment.
 * \param[in] mime_type  The MIME type of the data, if left empty, it will
 *            be determined on the fly.
 * \param[in] flags  A set of quoted_printable_encode() flags.
 */
void attachment::quoted_printable_encode_and_set_data(
                            std::string const & data
                          , std::string const & mime_type
                          , int flags)
{
    std::string const encoded_data(edhttp::quoted_printable_encode(data, flags));

    set_data(encoded_data, mime_type);

    add_header(edhttp::g_name_edhttp_field_content_transfer_encoding
             , edhttp::g_name_edhttp_param_quoted_printable);
}


/** \brief The email attachment data.
 *
 * This function retrieves the attachment data from this email attachment
 * object. This is generally UTF-8 characters when we are dealing with
 * text (HTML or plain text.)
 *
 * The data type is defined in the Content-Type header which is automatically
 * defined by the mime_type parameter of the set_data() function call. To
 * retrieve the MIME type, use the following:
 *
 * \code
 * QString mime_type(attachment->get_header(get_name(name_t::SNAP_NAME_CORE_CONTENT_TYPE_HEADER)));
 * \endcode
 *
 * \warning
 * This funtion returns the data by copy. Use with care (not repetitively?)
 *
 * \return A copy of this attachment's data.
 */
std::string attachment::get_data() const
{
    return f_data;
}


/** \brief Retrieve the value of a header.
 *
 * This function returns the value of the named header. If the header
 * is not currently defined, this function returns an empty string.
 *
 * \exception sendmail_exception_invalid_argument
 * The name of a header cannot be empty. This exception is raised if
 * \p name is empty.
 *
 * \param[in] name  A valid header name.
 *
 * \return The current value of that header or an empty string if undefined.
 */
std::string attachment::get_header(std::string const & name) const
{
    if(name.empty())
    {
        throw invalid_parameter("attachment::get_header(): Cannot retrieve a header with an empty name");
    }

    auto const it(f_headers.find(snapdev::to_case_insensitive_string(name)));
    if(it != f_headers.end())
    {
        return it->second;
    }

    return std::string();
}


/** \brief Add the Content-Disposition field.
 *
 * Helper function to add the Content-Disposition without having to
 * generate the string of the field by hand, especially because the
 * filename needs special care if defined.
 *
 * The disposition is expected to be of type "attachment" by default.
 * You may change that by changing the last parameter to this function.
 *
 * The function also accepts a filename and a date. If the date is set
 * to zero (default) then time() is used.
 *
 * \code
 *      email e;
 *      ...
 *      attachment a;
 *      a.set_data(some_pdf_buffer, "application/pdf");
 *      a.set_content_disposition("your-file.pdf");
 *      e.add_attachment(a);
 *      ...
 *      // if in a server plugin, you can use post_email()
 *      sendmail::instance()->post_email(e);
 * \endcode
 *
 * \attention
 * The \p filename parameter can include a full path although only the
 * basename including all extensions are saved in the header. The path
 * is not useful on the destination computer and can even possibly be
 * a security issue in some cases.
 *
 * \warning
 * The modification_date is an int64_t type in microsecond
 * as most often used in Snap! However, emails only use dates with
 * a one second precision so the milli and micro seconds will
 * generally be ignored.
 *
 * \param[in] filename  The name of this attachment file.
 * \param[in] modification_date  The last modification date of this file.
 *                               Defaults to zero meaning use 'now'.
 *                               Value is in seconds.
 * \param[in] attachment_type  The type of attachment, defaults to "attachment",
 *                             which is all you need in most cases.
 */
void attachment::set_content_disposition(
      std::string const & filename
    , int64_t modification_date
    , std::string const & attachment_type)
{
    // TODO: make use of a WeightedHTTPString::to_string() (class to be renamed!)

    // type
    //
    if(attachment_type.empty())
    {
        throw invalid_parameter("attachment::set_content_disposition(): The attachment type cannot be an empty string.");
    }
    std::string content_disposition(attachment_type);
    content_disposition += ';';

    // filename (optional)
    //
    std::string const basename(snapdev::pathinfo::basename(filename));
    if(!basename.empty())
    {
        // the path is not going to be used (should not be at least) for
        // security reasons we think it is better not to include it at all
        //
        content_disposition += ' ';
        content_disposition += edhttp::g_name_edhttp_param_filename;
        content_disposition += '=';
        content_disposition += edhttp::uri::urlencode(basename);
        content_disposition += ';';
    }

    // modificate-date
    //
    if(modification_date == 0)
    {
        modification_date = time(nullptr);
    }
    content_disposition += ' ';
    content_disposition += edhttp::g_name_edhttp_param_modification_date;
    content_disposition += "=\"";
    content_disposition += edhttp::date_to_string(
                                  modification_date
                                , edhttp::date_format_t::DATE_FORMAT_EMAIL);
    content_disposition += "\";";

    // save the result in the headers
    //
    add_header(
              edhttp::g_name_edhttp_field_content_disposition
            , content_disposition);
}


/** \brief Check whether a named header was defined in this attachment.
 *
 * Each specific attachment can be given a set of headers that are saved
 * at the beginning of that part in a multi-part email.
 *
 * This function is used to know whther a given header was already
 * defined or not.
 *
 * \note
 * The function returns true whether the header is properly defined or
 * is the empty string.
 *
 * \param[in] name  A valid header name.
 * \param[in] value  The value of this header.
 *
 * \sa set_data()
 */
bool attachment::has_header(std::string const & name) const
{
    if(name.empty())
    {
        throw invalid_parameter("attachment::has_header(): When check the presence of a header, the name cannot be empty.");
    }

    return f_headers.find(snapdev::to_case_insensitive_string(name)) != f_headers.end();
}


/** \brief Header of this attachment.
 *
 * Each attachment can be assigned a set of headers such as the Content-Type
 * (which is automatically set by the set_data() function.)
 *
 * Headers in an attachment are similar to the headers in the main email
 * only it cannot include certain entries such as the To:, Cc:, etc.
 *
 * In most cases you want to include the filename if the attachment represents
 * a file. Plain text and HTML will generally only need the Content-Type which
 * is already set by a call to the set_data() funciton.
 *
 * Note that the name of a header is case insensitive. So the names
 * "Content-Type" and "content-type" represent the same header. Which
 * one will be used when generating the output is a non-disclosed internal
 * functionality. You probably want to use the SNAP_SENDMAIL_HEADER_...
 * names anyway (at least for those that are defined.)
 *
 * \note
 * The Content-Transfer-Encoding is managed internally and you are not
 * expected to set this value. The Content-Disposition is generally set
 * to "attachment" for files that are attached to the email.
 *
 * \exception sendmail_exception_invalid_argument
 * The name of a header cannot be empty. This exception is raised if the name
 * is empty.
 *
 * \todo
 * As we develop a functioning version of sendmail we want to add tests to
 * prevent a set of fields that we will handle internally and thus we do
 * not want users to be able to set here.
 *
 * \param[in] name  A valid header name.
 * \param[in] value  The value of this header.
 *
 * \sa set_data()
 */
void attachment::add_header(std::string const & name, std::string const & value)
{
    if(name.empty())
    {
        throw invalid_parameter("attachment::add_header(): When adding a header, the name cannot be empty.");
    }

    f_headers[snapdev::to_case_insensitive_string(name)] = value;
}


/** \brief Remove a header.
 *
 * This function searches for the \p name header and removes it from the
 * list of defined headers. This is different from setting the value of
 * a header to the empty string as the header continues to exist.
 *
 * \param[in] name  The name of the header to get rid of.
 */
void attachment::remove_header(std::string const & name)
{
    auto const it(f_headers.find(snapdev::to_case_insensitive_string(name)));
    if(it != f_headers.end())
    {
        f_headers.erase(it);
    }
}


/** \brief Get all the headers defined in this email attachment.
 *
 * This function returns the map of the headers defined in this email
 * attachment. This can be used to quickly scan all the headers.
 *
 * \note
 * It is important to remember that since this function returns a reference
 * to the map of headers, it may break if you call add_header() while going
 * through the references unless you make a copy.
 *
 * \return A direct and constant reference to the internal header map.
 */
header_map_t const & attachment::get_all_headers() const
{
    return f_headers;
}


/** \brief Add a related sub-attachment.
 *
 * This function lets you add a related sub-attachment to an email
 * attachment. At this time, this is only accepted on HTML attachments
 * (body) to attach files such as images, CSS, and scripts.
 *
 * \note
 * At this time we prevent you from adding related sub-attachments to
 * already related sub-attachments. Note that emails can have more levels,
 * but we limit the body of the email (very first attachment) to either
 * Text or HTML. If HTML, then the sendmail plugin takes care of
 * adding the Text version. Thus the sendmail email structure is somewhat
 * different from the resulting email.
 *
 * The possible structure of a resulting email is:
 *
 * \code
 * - multipart/mixed
 *   - multipart/alternative
 *     - text/plain
 *     - multipart/related
 *       - text/html
 *       - image/jpg (Images used in text/html)
 *       - image/png
 *       - image/gif
 *       - text/css (the CSS used by the HTML)
 *   - application/pdf (PDF attachment)
 * \endcode
 *
 * The structure of the sendmail attachment for such an email would be:
 *
 * \code
 * - HTML attachment
 *   - image/jpg
 *   - image/png
 *   - image/gif
 *   - text/css
 * - application/pdf
 * \endcode
 *
 * Also, you are much more likely to use the set_email_path() which
 * means you do not have to provide anything more than than the dynamic
 * file attachments (i.e. the application/pdf file in our example here.)
 * Everything else is taken care of by the sendmail plugin.
 *
 * \param[in] data  The attachment to add to this attachment by copy.
 */
void attachment::add_related(attachment const & data)
{
    // if we are a sub-attachment, we do not accept a sub-sub-attachment
    //
    if(f_is_sub_attachment)
    {
        throw too_many_levels("attachment::add_related(): this attachment is already a related sub-attachment, you cannot add more levels");
    }

    // related sub-attachment limitation
    //
    if(data.get_related_count() != 0)
    {
        throw too_many_levels("attachment::add_related(): you cannot add a related sub-attachment to an attachment when that related sub-attachment has itself a related sub-attachment");
    }

    // create a copy of this attachment
    //
    // note that we do not attempt to use the shared pointer, we make a
    // full copy instead, this is because some people may end up wanting
    // to modify the attachment parameter and then add anew... what will
    // have to a be a different attachment.
    //
    attachment copy(data);

    // mark this as a sub-attachment to prevent users from adding
    // sub-sub-attachments to those
    //
    copy.f_is_sub_attachment = true;

    // save the result in this attachment sub-attachments
    //
    f_sub_attachments.push_back(copy);
}


/** \brief Return the number of sub-attachments.
 *
 * Attachments can be assigned related sub-attachments. For example, an
 * HTML page can be given images, CSS files, etc.
 *
 * This function returns the number of such sub-attachments that were
 * added with the add_attachment() function. The count can be used to
 * retrieve all the sub-attachments with the get_attachment() function.
 *
 * \return The number of sub-attachments.
 *
 * \sa add_related()
 * \sa get_related()
 */
int attachment::get_related_count() const
{
    return f_sub_attachments.size();
}


/** \brief Get one of the related sub-attachment of this attachment.
 *
 * This function is used to retrieve the related attachments found in
 * another attachment. These are called sub-attachments.
 *
 * These attachments are viewed as related documents to the main
 * attachment. These are used with HTML at this point to add images,
 * CSS files, etc. to the HTML files.
 *
 * \warning
 * The function returns a reference to the internal object. Calling
 * add_attachment() is likely to invalidate that reference.
 *
 * \exception out_of_range
 * If the index is out of range, this exception is raised.
 *
 * \param[in] index  The attachment index.
 *
 * \return A reference to the attachment.
 *
 * \sa add_related()
 * \sa get_related_count()
 */
attachment & attachment::get_related(int index) const
{
    if(static_cast<std::size_t>(index) >= f_sub_attachments.size())
    {
        throw libmimemail_out_of_range("attachment::get_related() called with an invalid index.");
    }
    return const_cast<attachment &>(f_sub_attachments[index]);
}


/** \brief Unserialize an email attachment.
 *
 * This function unserializes an email attachment that was serialized using
 * the serialize() function. This is considered an internal function as it
 * is called by the unserialize() function of the email object.
 *
 * \param[in] r  The reader used to read the input data.
 *
 * \sa serialize()
 */
void attachment::deserialize(snapdev::deserializer<std::stringstream> & in, bool is_sub_attachment)
{
    f_is_sub_attachment = is_sub_attachment;

    snapdev::deserializer<std::stringstream>::process_hunk_t func(std::bind(
                  &attachment::process_hunk
                , this
                , std::placeholders::_1
                , std::placeholders::_2));
    if(!in.deserialize(func))
    {
        SNAP_LOG_WARNING
            << "attachment unserialization stopped early."
            << SNAP_LOG_SEND;
    }
}


bool attachment::process_hunk(
      snapdev::deserializer<std::stringstream> & in
    , snapdev::field_t const & field)
{
    if(field.f_name == "header")
    {
        std::string value;
        in.read_data(value);
        f_headers[snapdev::to_case_insensitive_string(field.f_sub_name)] = value;
    }
    else if(field.f_name == "attachment")
    {
        attachment a;
        a.deserialize(in, true);
        add_related(a);
    }
    else if(field.f_name == "data")
    {
        in.read_data(f_data);
    }
    else
    {
        SNAP_LOG_WARNING
            << "attachement unserialization found unknown field \""
            << field.f_name
            << "\"; ignoring."
            << SNAP_LOG_SEND;
    }

    return true;
}


/** \brief Serialize an attachment to a writer.
 *
 * This function serialize an attachment so it can be saved in the database
 * in the form of a string.
 *
 * \param[in,out] w  The writer where the data gets saved.
 */
void attachment::serialize(snapdev::serializer<std::stringstream> & out) const
{
    for(auto const & it : f_headers)
    {
        out.add_value("header", snapdev::to_string(it.first), it.second);
    }
    for(auto const & it : f_sub_attachments)
    {
        snapdev::recursive sub_field(out, "attachment");
        it.serialize(out);
    }

    // note that f_data may be binary data
    //
    out.add_value("data", f_data);
}


/** \brief Compare two attachments against each others.
 *
 * This function compares two attachments against each other and returns
 * true if both are considered equal.
 *
 * \param[in] rhs  The right handside.
 *
 * \return true if both attachments are equal.
 */
bool attachment::operator == (attachment const & rhs) const
{
    return f_headers           == rhs.f_headers
        && f_data              == rhs.f_data
        && f_is_sub_attachment == rhs.f_is_sub_attachment
        && f_sub_attachments   == rhs.f_sub_attachments;
}





}
// namespace libmimetype
// vim: ts=4 sw=4 et

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   method_handling.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aoumad <aoumad@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/04/09 17:52:50 by aoumad            #+#    #+#             */
/*   Updated: 2023/05/24 17:09:46 by aoumad           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "respond.hpp"
# include <stdio.h>

void    Respond::handle_get_response(std::vector<server> servers)
{
    // step 2: check if it's a CGI or not (like if `index` of the configuration file has .py or .php...etc)
    if (_is_cgi == true)
    {
        if (access(get_file_cgi().c_str(), R_OK) != 0)
        {
            handle_error_response(servers, 403);
            return ;
        }
        run_cgi(r, *this, servers);
        return ;
    }
    // step 3: check if it's a file or not
    if (ft_check_file() == true)
    {
        ft_handle_file(servers);
        return ;
    }
    // step 4 : check the index in the configuration file and render it
    int rtn_index = ft_handle_index(servers);
    if (rtn_index == 0)
        return ;
    else if (rtn_index == 1)
    {
        if (ft_handle_autoindex(servers))
        {
            handle_error_response(servers, 403);
            return ;
        }
    }
    else if (rtn_index == 2)
    {
        handle_error_response(servers, 404);
        return ;
    }

}

void    Respond::handle_post_response(std::vector<server> server)
{
    // step 1: check if the request body is empty or not
    if (r.get_body().empty())
    {
        handle_error_response(server, 400);
        return ;
    }
    if (_is_cgi == false && (server[_server_index]._location[_location_index].get_upload_store().empty() && server[_server_index]._location[_location_index].get_upload() == false))
        return ;
    _upload_store_path = _rooted_path;
    _upload_store = server[_server_index]._location[_location_index].get_upload_store();
    struct stat st;
    if (stat(_upload_store_path.c_str(), &st) != 0)
    {
        mkdir(_upload_store_path.c_str(), 0777);
        // or return error
    }
    if (check_post_type() == "x-www-form-urlencoded")
    {
        if (_is_cgi == true)
        {
            run_cgi(r, *this, server);
            set_status_code(201);
            set_status_message(get_response_status(201));
            return ;
        }
        else
        {
            handle_urlencoded();
            if (create_decode_files() == 2)
            {
                handle_error_response(server, 400);
                return ;
            }
            std::string path = r.get_uri();
            std::string::size_type i = r.get_uri().find_last_of('/');
            if (i != std::string::npos)
                path = r.get_uri().substr(i);
            set_status_code(201);
            set_status_message(get_response_status(201));
            init_response_body(server, path, server[_server_index]._location[_location_index].get_root());
            return ;
        }
    }
    if (check_post_type() == "form-data")
    {
        if (handle_form_data(server) == 2)
        {
            handle_error_response(server, 400);
            return ;
        }
        set_status_code(201);
        set_status_message(get_response_status(201));
        return ;
    }
}

void    Respond::handle_urlencoded()
{
    std::string line = r.get_body();
    Url_encoded encoded_form;
    std::string::size_type pos = 0;
    std::string::size_type end_pos = 0;
    r.handleSpecialCharacters(line);
    while (pos != std::string::npos)
    {
        end_pos = line.find('&', pos);
        std::string pair = line.substr(pos, end_pos - pos);
        std::string::size_type pivot = pair.find("=");
        if (pivot != std::string::npos)
        {
            encoded_form.key = pair.substr(0, pivot);
            encoded_form.value = pair.substr(pivot + 1);
            _url_decode.push_back(encoded_form);
        }

        if (end_pos == std::string::npos)
            break;
        pos = end_pos + 1;
    }
}

int     Respond::create_decode_files()
{
    std::string file_name;
    std::string file_content;
    std::ofstream file;
    std::vector<Url_encoded>::iterator it = _url_decode.begin();
    
    while (it != _url_decode.end())
    {
        file_name = _upload_store;
        file_name += "/" + it->key;
        file.open(file_name.c_str());
        // Check if you have permission to access the file
        if (access(file_name.c_str(), R_OK) != 0)
            return (2);
        file << it->value;
        file.close();
        it++;
    }
    return (0);
}

int Respond::handle_form_data(std::vector<server> server)
{
    // Find the first boundary
    size_t  pos = r.get_body().find(_boundary);
    if (!pos)
        return (0);
    // Loop through the form data, locating boundaries and reading data betweem them
    while (true)
    {
        // find the next boundary
        pos = r.get_body().find(_boundary, pos);
        if (pos == std::string::npos)
            break;
        // Read the data between the boundaries
        FormData formData = read_form_data(server, pos); // escape /r/n
        if (_file_too_large == true)
        {
            handle_error_response(server, 413);
            return (0);
        }
        if (formData.isValid())
            _form_data.push_back(formData); // Add the form data to the list
        if (_last_boundary == true)
            break;
        pos += _boundary.length() + 2;
    }
    if (create_form_data() == 2)
    {
        handle_error_response(server, 400);
        return (2);
    }
    std::string path = r.get_uri();
    std::string::size_type i = r.get_uri().find_last_of('/');
    if (i != std::string::npos)
        path = r.get_uri().substr(i);
    init_response_body(server, path, server[_server_index]._location[_location_index].get_root());
    return (0);
}

int Respond::create_form_data()
{
    std::string file_name;
    std::string file_content;
    std::ofstream file;
    std::vector<FormData>::iterator it = _form_data.begin();

    while (it != _form_data.end())
    {
        if (it->get_content_type().empty())
        {
            it++;
            continue;
        }
        file_name = _upload_store;
        file_name += "/" + it->get_file_name();
        file.open(file_name.c_str());
        // Check if you have permission to access the file
        if (access(file_name.c_str(), R_OK) != 0)
            return (2);
        file << it->get_data();
        file.close();
        it++;
    }
    return (0);
}
// Helper function to locate the next boundary in the form data
size_t Respond::find_boundary(size_t pos)
{
    return (r.get_body().find(_boundary, pos));
}

FormData Respond::read_form_data(std::vector<server> servers ,size_t pos)
{
    FormData form_data;
    std::string line;
    std::string data;

    size_t start = r.get_body().find(_boundary, pos);
    if (start == std::string::npos)
        return form_data; // boundary not found

    // skip the boundary line
    start += _boundary.length() + 2;
    std::string last_boundary = _boundary + "--";
    size_t end = r.get_body().find(_boundary, start);
    if (end == std::string::npos)
    {
        return (form_data); // Boundary not found
    }

    size_t end_last = r.get_body().find(last_boundary, start);
    if (end_last != std::string::npos && end_last == end)
    {
        _last_boundary = true;
    }

    // Extracts the form data section
    std::string section = r.get_body().substr(start, end - start);
    // Process the headers
    size_t header_end = section.find("\r\n\r\n");
    if (header_end != std::string::npos)
    {
        std::string header_section = section.substr(0, header_end);
        // Find the form name in the headers
        size_t name_start = header_section.find("name=\"") + sizeof("name=\"") - 1;
        size_t name_end = header_section.find("\"", name_start);
        if (name_end != std::string::npos)
            form_data.name = header_section.substr(name_start, name_end - name_start);
        
        // Find the filename in the headers
        size_t filename_start = header_section.find("filename=\"") + sizeof("filename=\"") - 1;
        size_t filename_end = header_section.find("\"", filename_start);
        if (filename_end != std::string::npos)
            form_data.file_name = header_section.substr(filename_start, filename_end - filename_start);
        
        size_t content_type_start = header_section.find("Content-Type: ") + sizeof("Content-Type: ") - 1;
        form_data.content_type = header_section.substr(content_type_start);
    }

    // Process the data content
    std::string data_content = section.substr(header_end + 4); // Skip the CRLF delimiter
    
    if (data_content.length() * 8 >= (unsigned int)servers[_server_index].get_client_max_body_size())
        _file_too_large = true;
    
    form_data.data = data_content;
    return (form_data);
}

std::string Respond::check_post_type()
{
    if(r.get_header("Content-Type").find("multipart/form-data") != std::string::npos)
        return ("form-data");
    else if(r.get_header("Content-Type").find("application/x-www-form-urlencoded") != std::string::npos)
        return ("x-www-form-urlencoded");
    else
        return ("regular");
}

void    Respond::handle_delete_response(std::vector<server> server)
{
    if (std::remove(_rooted_path.c_str()) == 0)
    {
        _status_code = 200;
        _status_message = get_response_status(_status_code);
        set_header("Content-Type", r.get_header("Content-Type"));
        set_header("Content-Length", std::to_string(_response_body.length()));
        set_header("Connection", "keep-alive");
    }
    else
    {
        handle_error_response(server, 409);
    }
}
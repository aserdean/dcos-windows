/****************************** Module Header ******************************\
* Module Name:  ServiceBase.h
* Project:      CppWindowsService
* Copyright (c) Microsoft Corporation.
*
* Provides a class for performing logging according to the output types available in 
* systemd unit files.
*
* This source is subject to the Microsoft Public License.
* See http://www.microsoft.com/en-us/openness/resources/licenses.aspx#MPL.
* All other rights reserved.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
* EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
\***************************************************************************/
#ifndef __SYSTEMD_JOURNAL_H__
#define __SYSTEMD_JOURNAL_H__

#include <ios>
#include <ostream>
#include <fstream>
#include <iostream>
#include "windows.h"

namespace journalstreams {


    // StandardOut and StandardError are specified as type and maybe name
    // most of these will have significant differences in their semantics from unix/linux.
    enum OUTPUT_TYPE {
        OUTPUT_TYPE_INVALID,
        OUTPUT_TYPE_INHERIT,
        OUTPUT_TYPE_NULL,
        OUTPUT_TYPE_TTY,
        OUTPUT_TYPE_JOURNAL,
        OUTPUT_TYPE_SYSLOG,
        OUTPUT_TYPE_KMSG,
        OUTPUT_TYPE_JOURNAL_PLUS_CONSOLE,
        OUTPUT_TYPE_SYSLOG_PLUS_CONSOLE,
        OUTPUT_TYPE_KMSG_PLUS_CONSOLE,
        OUTPUT_TYPE_FILE,   // requires a path
        OUTPUT_TYPE_SOCKET,
        OUTPUT_TYPE_FD      // requires a name
    };
    
    enum LOGGING_MESSAGE_PRIROITY {
        LOGGING_LEVEL_NONE = 0,
        LOGGING_LEVEL_DEBUG,
        LOGGING_LEVEL_INFO,
        LOGGING_LEVEL_NOTICE,
        LOGGING_LEVEL_WARNING,
        LOGGING_LEVEL_ERROR,
        LOGGING_LEVEL_CRITICAL,
        LOGGING_LEVEL_ALERT,
        LOGGING_LEVEL_EMERGENCY
    };
    
    // For output type null
    
    template <class cT, class traits = std::char_traits<cT> >
    class basic_nullstreambuf : public std::basic_streambuf<cT, traits>
    {
    public:
        basic_nullstreambuf() {}
        ~basic_nullstreambuf() {}
        typename traits::int_type overflow(typename traits::int_type c) override
        {
            std::cerr << "null stream" << std::endl;
            return traits::not_eof(c); // indicate success
        }
    };
    
    template <class cT, class traits = std::char_traits<cT> >
    class basic_onullstream : public std::basic_ostream<cT, traits>
    {
        basic_nullstreambuf<cT> buf;
    public:
        basic_onullstream()
            : std::ostream(&buf)
            , buf()
        {
        }
    };
    
    
    typedef basic_onullstream<char> onullstream;
    typedef basic_onullstream<wchar_t> wonullstream;
    
    template <class cT, class traits = std::char_traits<cT> >
    class basic_journalstreambuf: public std::basic_streambuf<cT, traits>
    {
    public:
        basic_journalstreambuf() { 
            static const char *labels[] =  {
                                         "NO_LOG_LEVEL",
                                         "DEBUG",
                                         "VERBOSE",
                                         "INFO",
                                         "WARNING",
                                         "ERROR",
                                         "FATAL" };
            m_current = m_buffer;
            m_filehandle = INVALID_HANDLE_VALUE;
            m_console_output  = false;
            m_file_output     = false;
            m_eventlog_output = false;
            m_console = std::wofstream("c:/var/log/systemd.log", std::ofstream::app);
            m_logginglevel    =  0;
            m_default_msglevel = 3; // INFO
            m_msglevel      = m_default_msglevel;
            LOG_LEVEL_LABELS = labels;
    
        };
    
        basic_journalstreambuf(const basic_journalstreambuf<cT, traits> &&from):
           std::basic_streambuf<cT, traits>(std::move(from)) {
            static const char *labels[] =  {
                                         "NO_LOG_LEVEL",
                                         "DEBUG",
                                         "VERBOSE",
                                         "INFO",
                                         "WARNING",
                                         "ERROR",
                                         "FATAL" };
            m_current = m_buffer;
            size_t offset = from.m_current-from.m_buffer;
    
            memcpy(this->m_buffer, from.m_buffer, sizeof(from.m_buffer));
            this->m_current    = this->m_buffer+offset;
            this->m_filehandle = from.m_filehandle;
            this->m_console_output  = from.m_console_output;
            this->m_file_output     = from.m_file_output;
            this->m_eventlog_output = from.m_eventlog_output;
            this->m_logginglevel    =  from.m_logginglevel;
            this->m_default_msglevel = from.m_default_msglevel;
            this->m_msglevel      = from.m_msglevel;
            m_console = std::wofstream("c:/var/log/systemd.log", std::ofstream::app);
            LOG_LEVEL_LABELS = labels;
        };
    
        ~basic_journalstreambuf() { };
        
        void set_handle(HANDLE handle) { m_filehandle = handle; };
        void set_output_console(boolean output_to_console) { m_console_output = output_to_console; };
        void set_output_logfile(boolean output_to_log) { m_file_output = output_to_log; };
        void set_output_eventlog(boolean output_to_eventlog) { m_eventlog_output = output_to_eventlog; };
        void set_logginglevel(int lev) { m_logginglevel = lev; };
        void set_msglevel(int lev) { m_msglevel = lev; };
        void set_default_msglevel(int lev) { m_default_msglevel = lev; };
    
        typename traits::int_type overflow(typename traits::int_type wc) override
        {
            char c = (char) wc;
            
            if (m_current < m_bufferlimit) {
                *m_current++ = c;
            }
            if (c == '\n') {
                *m_current = '\0';
    
                if (m_msglevel >= m_logginglevel ) {
                    std::wcerr << m_buffer << std::endl;
                    if (m_console_output) {
                        m_console << m_buffer;
                        m_console.flush();
                    }
                    if (m_file_output && m_filehandle != INVALID_HANDLE_VALUE) {
                          DWORD result = WriteFile(m_filehandle, m_buffer, (DWORD)(m_current-m_buffer)*sizeof(m_buffer[0]), NULL, NULL);
                        FlushFileBuffers(m_filehandle);
                    }
                }
                m_current = m_buffer;
                m_msglevel = m_default_msglevel;
            }
            return traits::not_eof(c); // indicate success
        };
    
        virtual int sync() override
        {
            if (m_current > m_buffer) {
                *m_current = '\0';
                 if (m_msglevel >= m_logginglevel) {
                     std::wcerr << L"journal stream: sync :" << m_buffer << std::endl;
                     if (m_console_output) {
                         m_console << m_buffer;
                     }
                     if (m_file_output && m_filehandle != INVALID_HANDLE_VALUE) {
                          DWORD result = WriteFile(m_filehandle, m_buffer, (DWORD)(m_current-m_buffer)*sizeof(m_buffer[0]), NULL, NULL);
                     }
                 }
                 m_current = m_buffer;
                 m_msglevel = m_default_msglevel;
            }
            return 0;
        };
         
    
    protected:
        static const int MAX_BUFFER_SIZE = 2048;
        friend class wojournalstream; 
        std::wofstream m_console;
        HANDLE m_filehandle;
        boolean m_console_output;
        boolean m_file_output;
        boolean m_eventlog_output;
        int     m_logginglevel; // The usual debug, info, warning, error, fatal
        int     m_msglevel; // The usual debug, info, warning, error, fatal
        int     m_default_msglevel;
    
    private:
        char m_buffer[MAX_BUFFER_SIZE+1]; // Added one for final null
        char *m_current;
        const char *m_bufferlimit = m_buffer + MAX_BUFFER_SIZE;
    
        const char **LOG_LEVEL_LABELS;
    };
    
    
    // We create a type here to differentiate the operator
    
    struct _Loglevel { int level; };
    
    // Then a standalone func to return it. Its does nothing but return the type
    inline _Loglevel loglevel(int n) 
    {
        return { n };
    }
    
    inline _Loglevel Debug() 
    
    {
        return { 1 };
    }
    
    inline _Loglevel Verbose() 
    
    {
        return { 2 };
    }
    
    inline _Loglevel Info() 
    
    {
        return { 3 };
    }
    
    inline _Loglevel Warning() 
    
    {
        return { 4 };
    }
    
    inline _Loglevel Error() 
    
    {
        return { 5 };
    }
    
    inline _Loglevel FATAL() 
    
    {
        return { 6 };
    }
    
    class wojournalstream: 
            public std::basic_ostream<wchar_t> {
            basic_journalstreambuf<wchar_t> buf;
    
        public:
            wojournalstream():
                std::basic_ostream<wchar_t>(&buf),
                buf()
            {
            };
     
            virtual ~wojournalstream() {
                if (m_filehandle != INVALID_HANDLE_VALUE) {
                    CloseHandle(m_filehandle);
                    m_filehandle = INVALID_HANDLE_VALUE;
                }
            }
    
            wojournalstream(std::wstring output_type, std::wstring path );
            HANDLE GetHandle() { return m_filehandle;  };
            virtual boolean is_open() {
                return m_filehandle != INVALID_HANDLE_VALUE;
            };
            
            virtual int setmsglevel(int lev) { buf.set_msglevel(lev); return lev; }
            virtual int set_default_msglevel(int lev) { buf.set_default_msglevel(lev); return lev; }
            virtual int setlogginglevel(int lev) { buf.set_logginglevel(lev); return lev; }
    
            virtual void open(const std::wstring output_type, const std::wstring path);
    
            virtual void close() {
                if (m_filehandle != INVALID_HANDLE_VALUE) {
                    CloseHandle(m_filehandle);
                    m_filehandle = INVALID_HANDLE_VALUE;
                }
                m_output_type = OUTPUT_TYPE_NULL;
                buf.set_handle(INVALID_HANDLE_VALUE);
                buf.set_output_console(false);
                buf.set_output_logfile(false);
                buf.set_output_eventlog(false);
            };
        
        private:
            HANDLE m_filehandle;
            enum OUTPUT_TYPE m_output_type;
    };
    
    inline wojournalstream&
    operator<<(wojournalstream& wos,  _Loglevel l)
    {
        wos.setmsglevel(l.level);
        return wos;
    }

}

#endif

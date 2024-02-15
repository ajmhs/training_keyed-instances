/*
* (c) Copyright, Real-Time Innovations, 2020.  All rights reserved.
* RTI grants Licensee a license to use, modify, compile, and create derivative
* works of the software solely for use with RTI Connext DDS. Licensee may
* redistribute copies of the software provided that all such copies are subject
* to this license. The software is provided "as is", with no warranty of any
* type, including any warranty for fitness for any purpose. RTI is under no
* obligation to maintain or support the software. RTI shall not be liable for
* any incidental or consequential damages arising out of the use or inability
* to use the software.
*/

#include <algorithm>
#include <sstream>
#include <deque>

#include <dds/sub/ddssub.hpp>
#include <dds/core/ddscore.hpp>
#include <rti/config/Logger.hpp>  // for logging
// alternatively, to include all the standard APIs:
//  <dds/dds.hpp>
// or to include both the standard APIs and extensions:
//  <rti/rti.hpp>
//
// For more information about the headers and namespaces, see:
//    https://community.rti.com/static/documentation/connext-dds/7.2.0/doc/api/connext_dds/api_cpp2/group__DDSNamespaceModule.html
// For information on how to use extensions, see:
//    https://community.rti.com/static/documentation/connext-dds/7.2.0/doc/api/connext_dds/api_cpp2/group__DDSCpp2Conventions.html


#include <ncurses.h>

#include "shapes.hpp"
#include "application.hpp"  // for command line parsing and ctrl-c

using std::cout;
using std::endl;
using std::deque;
using std::string;
using std::stringstream;

#define COLOR_PURPLE COLOR_WHITE + 1
#define COLOR_ORANGE COLOR_WHITE + 2

static deque<string> log_data;
static const int log_y = 20;
void display_log(const string& logline) {

    log_data.push_back(logline);
    if (log_data.size() > 5)
        log_data.pop_front();

    int cur_y = log_y;
    for (const auto &s : log_data) {
        mvaddstr(cur_y++, 0, s.c_str());
    }

    refresh();    
}

void display_sample(const ShapeTypeExtended& shape) {
    
    for (int c = colours::MIN_COLOUR; c != colours::MAX_COLOUR; c++) {
        if (0 == shape.color().compare(colours::ToStr[c])) {

            if (has_colors()) {
                attron(COLOR_PAIR(c));
                if (c == colours::YELLOW || c == colours::ORANGE)
                    attron(A_BOLD);
            }

            mvaddstr(c, 0, shape.color().c_str());
            if (has_colors()) {
                attroff(COLOR_PAIR(c));
                attroff(A_BOLD);
            }
            
            stringstream ss;
            ss << shape;
            mvaddstr(c, 10, ss.str().c_str());
            refresh();

            break;
        }
    }
}

int process_data(dds::sub::DataReader< ::ShapeTypeExtended> reader)
{
    stringstream ss;
    // Take all samples
    int count = 0;
    dds::sub::LoanedSamples< ::ShapeTypeExtended> samples = reader.take();
    for (auto sample : samples) {
        if (sample.info().valid()) {                                     
            count++;
            display_sample(sample.data());
            //std::cout << sample.data() << std::endl;            
        } 
        else {
            ss.str("");
            ShapeTypeExtended key_shape;
            reader.key_value(key_shape, sample.info().instance_handle());

            if (dds::sub::status::InstanceState::not_alive_no_writers() == sample.info().state().instance_state() &&
                dds::sub::status::SampleState::not_read() == sample.info().state().sample_state()) {

                ss << "Instance with key " << key_shape.color() << " has dropped from the databus";
                display_log(ss.str());
            }
            else {
                // Announce other instance state changes
                ss << "Instance with key " << key_shape.color() << " changed to " << 
                    sample.info().state().instance_state();
                display_log(ss.str());
            }
        }
    }

    return count; 
} // The LoanedSamples destructor returns the loan

void run_subscriber_application(unsigned int domain_id, unsigned int sample_count)
{
    // DDS objects behave like shared pointers or value types
    // (see https://community.rti.com/best-practices/use-modern-c-types-correctly)

    // Start communicating in a domain, usually one participant per application
    dds::domain::DomainParticipant participant(domain_id);

    // Create a Topic with a name and a datatype
    dds::topic::Topic< ::ShapeTypeExtended> topic(participant, "Square");

    // Create a Subscriber and DataReader with default Qos
    dds::sub::Subscriber subscriber(participant);
    dds::sub::DataReader< ::ShapeTypeExtended> reader(subscriber, topic);

    // Create a ReadCondition for any data received on this reader and set a
    // handler to process the data
    unsigned int samples_read = 0;
    dds::sub::cond::ReadCondition read_condition(
        reader,
        dds::sub::status::DataState::any(),
        [reader, &samples_read]() { samples_read += process_data(reader); });

    // WaitSet will be woken when the attached condition is triggered
    dds::core::cond::WaitSet waitset;
    waitset += read_condition;

    while (!application::shutdown_requested && samples_read < sample_count) {
        //display_log("ShapeTypeExtended subscriber sleeping up to 1 sec...");

        // Run the handlers of the active conditions. Wait for up to 1 second.
        waitset.dispatch(dds::core::Duration(1));
    }
}

int main(int argc, char *argv[])
{

    using namespace application;

    initscr();
    cbreak();
    noecho();

    if (has_colors()) {
        start_color();
        init_color(COLOR_PURPLE, 128, 0, 128);
        init_color(COLOR_ORANGE, 255, 165, 0);

        init_pair(colours::PURPLE, COLOR_PURPLE, COLOR_BLACK);
        init_pair(colours::BLUE, COLOR_BLUE, COLOR_BLACK);
        init_pair(colours::RED, COLOR_RED, COLOR_BLACK);
        init_pair(colours::GREEN, COLOR_GREEN, COLOR_BLACK);
        init_pair(colours::YELLOW, COLOR_YELLOW, COLOR_BLACK);
        init_pair(colours::CYAN, COLOR_CYAN, COLOR_BLACK);
        init_pair(colours::MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
        init_pair(colours::ORANGE, COLOR_ORANGE, COLOR_BLACK);
    }

    clear();

    // Parse arguments and handle control-C
    auto arguments = parse_arguments(argc, argv);
    if (arguments.parse_result == ParseReturn::exit) {
        return EXIT_SUCCESS;
    } else if (arguments.parse_result == ParseReturn::failure) {
        return EXIT_FAILURE;
    }
    setup_signal_handlers();

    // Sets Connext verbosity to help debugging
    rti::config::Logger::instance().verbosity(arguments.verbosity);

    try {
        run_subscriber_application(arguments.domain_id, arguments.sample_count);
    } catch (const std::exception& ex) {
        // This will catch DDS exceptions
        std::cerr << "Exception in run_subscriber_application(): " << ex.what()
        << std::endl;
        return EXIT_FAILURE;
    }

    // Releases the memory used by the participant factory.  Optional at
    // application exit
    dds::domain::DomainParticipant::finalize_participant_factory();

    endwin();

    return EXIT_SUCCESS;
}

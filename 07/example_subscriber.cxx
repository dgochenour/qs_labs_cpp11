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
#include <iostream>

#include <dds/sub/ddssub.hpp>
#include <dds/core/ddscore.hpp>
#include <rti/config/Logger.hpp>  // for logging

#include "example.hpp"
#include "application.hpp"  // for command line parsing and ctrl-c

//  LAB #4 - add DataReader listener to note Qos Error.
class MyReaderListener : public dds::sub::NoOpDataReaderListener<acme::Pose> {
    
public:

    MyReaderListener() { }

    void on_requested_incompatible_qos(
            dds::sub::DataReader<acme::Pose>& reader, 
            const dds::core::status::RequestedIncompatibleQosStatus & status) {

        std::cout << "Incompatible Offered QoS: " << std::endl;
        std::cout << "   Total Count: " << status.total_count() << std::endl;
        std::cout << "   Total Count Change: " << status.total_count_change() << std::endl;
        std::cout << "   Last Policy ID: " << status.last_policy_id() << std::endl;
    }

};

int process_data(dds::sub::DataReader<acme::Pose> reader)
{
    // Take all samples
    int count = 0;
    dds::sub::LoanedSamples<acme::Pose> samples = reader.take();
    for (const auto& sample : samples) {
        if (sample.info().valid()) {
            count++;
            std::cout << sample.data() << std::endl;
        } else {
            std::cout << "Instance state changed to "
            << sample.info().state().instance_state() << std::endl;
        }
    }

    return count; 
} // The LoanedSamples destructor returns the loan

void run_subscriber_application(unsigned int domain_id, unsigned int sample_count)
{
    // LAB #3 - remove string literals, use const strings from IDL
    // LAB #2 - Create a (non-default) qosProvider, then create the entities 
    // using explicitly names QoS profiles
	std::string qosProfile;
	qosProfile.clear();
	qosProfile.append(acme::qos_library).append("::").append(acme::qos_profile);
	dds::core::QosProvider qosProvider("file://MY_QOS_PROFILES.xml", qosProfile);

	dds::domain::DomainParticipant participant(domain_id, qosProvider.participant_qos());

    // LAB #3 - remove string literal, use const string from IDL
    dds::topic::Topic<acme::Pose> topic (participant, acme::pose_topic_name);

    dds::sub::Subscriber subscriber(participant, qosProvider.subscriber_qos());

    // LAB #4 - instantiate listener and create reader with listener
    MyReaderListener listener;

    // LAB #7 - use a Content Filtered Topic
    dds::topic::ContentFilteredTopic<acme::Pose> cft_topic = dds::core::null;
    cft_topic = dds::topic::ContentFilteredTopic<acme::Pose>(
            topic,
            "ContentFilteredTopic",
            dds::topic::Filter("position.x >= 0"));

    dds::sub::DataReader<acme::Pose> reader(
            subscriber, 
            cft_topic, //topic,
            qosProvider.datareader_qos(), 
            &listener, 
            dds::core::status::StatusMask::requested_incompatible_qos());

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

        std::cout << "acme::Pose subscriber sleeping up to 1 sec..." << std::endl;

        // Run the handlers of the active conditions. Wait for up to 1 second.
        waitset.dispatch(dds::core::Duration(1));
    }
}

int main(int argc, char *argv[])
{

    using namespace application;

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

    return EXIT_SUCCESS;
}

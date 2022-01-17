#ifndef SWITCH_HPP
#define SWITCH_HPP

#include "dds/dds.hpp"
#include <string>

namespace validation {

bool validate(
    dds::domain::DomainParticipant &participant,
    dds::sub::Subscriber &subscriber,
    unsigned long n_samples,
    const std::string &type_name);

}  //namespace validation

#endif //SWITCH_HPP

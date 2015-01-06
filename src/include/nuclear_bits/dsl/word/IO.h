/*
 * Copyright (C) 2013 Trent Houliston <trent@houliston.me>, Jake Woods <jake.f.woods@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef NUCLEAR_DSL_WORD_IO_H
#define NUCLEAR_DSL_WORD_IO_H

#include "nuclear_bits/dsl/word/emit/Direct.h"
#include "nuclear_bits/dsl/word/Single.h"
#include "nuclear_bits/dsl/store/ThreadStore.h"
#include "nuclear_bits/util/generate_callback.h"
#include "nuclear_bits/util/get_identifier.h"

namespace NUClear {
    namespace dsl {
        namespace word {
            
            struct IOConfiguration {
                int fd;
                int events;
                std::shared_ptr<threading::Reaction> reaction;
            };
            
            struct UnbindIO {
                uint64_t reactionId;
            };

            // IO is implicitly single
            struct IO : public Single {
                
                enum Events {
                    READ = 1,
                    WRITE = 2,
                    CLOSE = 4,
                    ERROR = 8
                };
                
                template <typename DSL, typename TFunc>
                static inline std::vector<threading::ReactionHandle> bind(Reactor& reactor, const std::string& label, TFunc&& callback, int fd, int watchSet) {
                    
                    // Make our callback generator
                    auto task = util::generate_callback<DSL>(std::forward<TFunc>(callback));
                    
                    // Get our identifier string
                    std::vector<std::string> identifier = util::get_identifier<typename DSL::DSL, TFunc>(label);
                    
                    // Get our powerplant
                    auto& powerplant = reactor.powerplant;
                    
                    // Create our unbinder
                    auto unbinder = [&powerplant] (threading::Reaction& r) {
                        powerplant.emit<emit::Direct>(std::make_unique<UnbindIO>(UnbindIO { r.reactionId }));
                    };
                    
                    // Create our reaction
                    auto reaction = std::make_shared<threading::Reaction>(identifier, task, DSL::precondition, DSL::postcondition, unbinder);
                    threading::ReactionHandle handle(reaction.get());
                    
                    // Send our configuration out
                    powerplant.emit<emit::Direct>(std::make_unique<IOConfiguration>(IOConfiguration {
                        fd,
                        watchSet,
                        std::move(reaction)
                    }));
                    
                    // Return our handles
                    std::vector<threading::ReactionHandle> handles = { handle };
                    return handles;
                }
                
                template <typename DSL>
                static inline std::tuple<int, int> get(threading::ReactionTask&) {
                    
                    // Return our thread local variable
                    return std::make_tuple(store::ThreadStore<int, 0>::value, store::ThreadStore<int, 1>::value);
                }
            };
        }
    }
}

#endif
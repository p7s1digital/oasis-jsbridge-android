/*
 * Copyright (C) 2019 ProSiebenSat1.Digital GmbH.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _JSBRIDGE_REFCOUNTER_H
#define _JSBRIDGE_REFCOUNTER_H

// Simple, thread-unsafe reference counter.
class RefCounter {

public:
    RefCounter() = default;
    RefCounter(const RefCounter &) = delete;
    RefCounter& operator=(const RefCounter &) = delete;

    void increment() { ++m_count; }
    void decrement() { --m_count; }
    bool isZero() const { return m_count == 0; }

private:
    int m_count = 0;
};

#endif //OASIS_NATIVE_ANDROID_REFCOUNTER_H

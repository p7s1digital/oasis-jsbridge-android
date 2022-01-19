package de.prosiebensat1digital.oasisjsbridge;

import de.prosiebensat1digital.oasisjsbridge.TestAidlCallback;
import de.prosiebensat1digital.oasisjsbridge.TestAidlParcelable;

interface TestAidlInterface {
    void hello(in TestAidlCallback cb);
    void servus(in TestAidlParcelable payload);
    TestAidlParcelable getValue();
}

package de.prosiebensat1digital.oasisjsbridge;

import de.prosiebensat1digital.oasisjsbridge.TestAidlCallback;
import de.prosiebensat1digital.oasisjsbridge.TestAidlEnum;
import de.prosiebensat1digital.oasisjsbridge.TestAidlParcelable;

interface TestAidlInterface {
    void triggerCallback(in TestAidlCallback cb);
    void setParcelable(in TestAidlParcelable p);
    void setEnum(in TestAidlEnum e);
    TestAidlParcelable getParcelable();
    TestAidlEnum getEnum();
}

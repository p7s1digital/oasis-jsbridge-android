package de.prosiebensat1digital.oasisjsbridge;

import de.prosiebensat1digital.oasisjsbridge.TestAidlCallback;
import de.prosiebensat1digital.oasisjsbridge.TestAidlEnum;
import de.prosiebensat1digital.oasisjsbridge.TestAidlParcelable;

interface TestAidlInterface {
    void triggerCallback(in TestAidlCallback cb);
    void setParcelable(in TestAidlParcelable p);
    void setParcelableArray(in TestAidlParcelable[] pa);
    void setParcelableList(in List<TestAidlParcelable> pl);
    void setParcelableListWithoutGeneric(in List pl);
    void setEnum(in TestAidlEnum e);
    TestAidlParcelable getParcelable();
    TestAidlEnum getEnum();
}

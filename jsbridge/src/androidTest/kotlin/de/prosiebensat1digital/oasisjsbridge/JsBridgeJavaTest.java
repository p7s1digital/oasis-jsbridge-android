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
package de.prosiebensat1digital.oasisjsbridge;

import org.junit.After;
import org.junit.BeforeClass;
import org.junit.Test;

import androidx.test.platform.app.InstrumentationRegistry;
import timber.log.Timber;
import static org.junit.Assert.assertEquals;

import android.content.Context;

// Minimal test for using JsBridge from Java
public final class JsBridgeJavaTest {
    private JsBridge jsBridge;
    private final Context context = InstrumentationRegistry.getInstrumentation().getContext();
    @BeforeClass
    static public void setUpClass() {
        Timber.plant(new Timber.DebugTree());
    }

    @After
    public void cleanUp() {
        if (jsBridge != null) {
            jsBridge.release();
        }
    }

    @Test
    public void testEvaluateNoRetVal() {
        // GIVEN
        JsBridge subject = createAndSetUpJsBridge();

        // WHEN
        subject.evaluateUnsync("console.log('test message');");
    }

    @Test
    public void testEvaluateString() {
        // GIVEN
        JsBridge subject = createAndSetUpJsBridge();
        String js = "1.5 + 2";

        // WHEN
        Double sumDouble = (Double) subject.evaluateBlocking(js, null);
        Integer sumInt = (Integer) subject.evaluateBlocking(js, Integer.class);
        String sumString = (String) subject.evaluateBlocking("\"" + js + "\"", null);

        // THEN
        assertEquals(sumDouble, new Double(3.5));
        assertEquals(sumInt, new Integer(3));
        assertEquals(sumString, "1.5 + 2");
    }

    interface NativeApi extends JsToNativeInterface {
        int calcSum(int a, int b);
    }

    @Test
    public void testRegisterJsToNative() {
        // GIVEN
        JsBridge subject = createAndSetUpJsBridge();
        NativeApi nativeApi = new NativeApi() {
            @Override
            public int calcSum(int a, int b) {
                return a + b;
            }
        };

        // WHEN
        JsValue nativeApiJsValue = JsValue.createJsToNativeProxy(subject, nativeApi, NativeApi.class);

        // THEN
        Integer sum = (Integer) subject.evaluateBlocking(
                "var nativeApi = " + nativeApiJsValue + ";\n" +
                    "nativeApi.calcSum(2, 3);\n",
                Integer.class
        );

        // THEN
        assertEquals(sum, new Integer(5));
    }

    interface JsApi extends NativeToJsInterface {
        int calcSum(int a, int b);
    }

    @Test
    public void testRegisterNativeToJs() {
        // GIVEN
        JsBridge subject = createAndSetUpJsBridge();
        JsValue nativeApiJsValue = new JsValue(subject,
                "({\n" +
                        "  calcSum: function(a, b) { return a + b; }\n" +
                        "})\n"
        );

        // THEN
        JsApi jsApi = nativeApiJsValue.createNativeToJsProxy(JsApi.class);
        int sum = jsApi.calcSum(6, 4);

        // THEN
        assertEquals(sum, 10);
    }


    // Private methods
    // ---

    private JsBridge createAndSetUpJsBridge() {
        JsBridge jsBridge = new JsBridge(JsBridgeConfig.standardConfig(), context);
        this.jsBridge = jsBridge;
        return jsBridge;
    }
}

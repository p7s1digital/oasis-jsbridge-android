package de.prosiebensat1digital.oasisjsbridge

/**
 * Android gradle plugin does not support JUnit @Category annotation by default,
 * so here is another way to group/scope tests by features:
 *
 * 1. Add custom annotation to your test methods.
 * 2. Update run config:
 *   Run -> Edit Configuration -> Android Instrumentation Tests -> Instrumentation Arguments
 *   name: annotation
 *   value: de.prosiebensat1digital.oasisjsbridge.Feature_SetTimeout
 *
 * 3. Run the tests with this config, JUnit will only run annotated tests
 *
 * see https://stackoverflow.com/a/38676843/1237733
 */
annotation class Feature_SetTimeout()
globalThis.createApi = function(javaApi, config) {
  return {
    createMessage: function(name) {
      const platformName = javaApi.getPlatformName();
      return javaApi.getTemperatureCelcius().then(function(celcius) {
        const value = config.useFahrenheit ? celcius * x + c : celcius;
        const unit = config.useFahrenheit ? "degrees F" : "degrees C";
        const valueStr = (Math.round(value * 10) / 10).toFixed(1);  // show only 1 decimal
        return "Hello " + platformName + "! The temperature is " + valueStr + " " + unit + ".";
      });
    },
    calcSum: function(a, b) {
      return new Promise(function(resolve) { resolve(a + b); });
    }
  };
}

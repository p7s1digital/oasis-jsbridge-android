globalThis.createApi = function(nativeApi, config) {
  return {
    createMessage: function(name) {
      const platformName = nativeApi.getPlatformName();
      return nativeApi.getTemperatureCelcius().then(function(celcius) {
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

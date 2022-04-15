import { createStore } from 'vuex'

export default createStore({
  state: {
    socket: {
      isConnected: false,
      reconnectError: false,
    },
    rules: {
      required: (value) => !!value || value == "0" || "Required.",
      max20Chars: (value) => value.length <= 20 || "Max 20 characters",
      email: (value) => {
          const pattern = /^(([^<>()[\]\\.,;:\s@"]+(\.[^<>()[\]\\.,;:\s@"]+)*)|(".+"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/;
          return pattern.test(value) || "Invalid e-mail.";
      },
      min0: (value) => value >= 0 || "Must be greater than or equal to 0",
      max255: (value) => value <= 255 || "Must be less than or equal to 255",
      minMinus12: (value) => value >= -12 || "Must be greater than or equal to -12",
      max14: (value) => value <= 14 || "Must be less than or equal to 14",
      portRange: (value) => (value > 0 && value <= 65535) || "Must be between 1 and 65535",
      volumeRange: (value) => (value > 0 && value <= 30) || "Must be between 1 and 30",
      // Can not be solved like this, we do not have access to the store.
      //   noPinDuplicates: value => ((   //dirty code, but creating an array and running a loop seems to be over-engineering
      //       this.config.DFPRXpin!=this.config.DFPTXpin && this.config.DFPRXpin!=this.config.BMESDAPin && this.config.DFPRXpin!=this.config.BMESCLPin && this.config.DFPRXpin!=this.config.DHTPin
      //    && this.config.DFPTXpin!=this.config.BMESDAPin && this.config.DFPTXpin!=this.config.BMESCLPin && this.config.DFPTXpin!=this.config.DHTPin
      //    && this.config.BMESDAPin!=this.config.BMESCLPin && this.config.BMESDAPin!=this.config.DHTPin
      //   //DHT pin and BME-SCL-pin may be identical!
      //  ) || 'Pin assignment must be unique'),
    },
    navLinks: [
      { title: "Dashboard", icon: "mdi-memory", page: "/" },
      { title: "Options", icon: "mdi-tune-vertical", page: "/options" },
      { title: "Sensors & Buttons", icon: "mdi-gesture-tap-button", page: "/sensorsbuttons" },
      { title: "Test Area", icon: "mdi-cube-outline", page: "/testarea" },
      { title: "Update", icon: "mdi-tray-arrow-up", page: "/update" },
      //{ title: "Pixel Gallery", icon: "mdi-image-outline", url: "https://pixelit.bastelbunker.de/PixelGallery", target: "_blank" },
      { title: "Pixel Gallery", icon: "mdi-image-outline", page: "/gallery" },
      { title: "Pixel Creator", icon: "mdi-pencil-box-outline", url: "https://pixelit.bastelbunker.de/PixelCreator", target: "_blank" },
      //{ title: "Pixel Creator", icon: "mdi-pencil-box-outline", page: "/creator" },
      { title: "Forum", icon: "mdi-forum-outline", url: "https://github.com/o0shojo0o/PixelIt/discussions", target: "_blank" },
      { title: "Blog", icon: "mdi-post-outline", url: "https://www.bastelbunker.de/pixel-it/", target: "_blank" },
      { title: "Documentation", icon: "mdi-book-open-page-variant-outline", url: "https://docs.bastelbunker.de/pixelit/", target: "_blank" },
      { title: "GitHub", icon: "mdi-github", url: "https://github.com/o0shojo0o/PixelIt", target: "_blank" },
    ],
    logData: [],
    sensorData: [],
    buttonData: [],
    sysInfoData: [],
    configData: {},
    matrixTypes: [
      { text: "Type 1 - Colum major", value: 1 },
      { text: "Type 2 - Row major", value: 2 },
      { text: "Type 3 - Tiled 4x 8x8 CJMCU", value: 3 },
    ],
    matrixCorrection: [
      { text: "Default", value: "default" },
      { text: "Typical SMD 5050", value: "typicalsmd5050" },
      { text: "Typical 8mm Pixel", value: "typical8mmpixel" },
      { text: "Tungsten 40W", value: "tungsten40w" },
      { text: "Tungsten 100W", value: "tungsten100w" },
      { text: "Halogen", value: "halogen" },
      { text: "Carbon Arc", value: "carbonarc" },
      { text: "High Noon Sun", value: "highnoonsun" },
      { text: "Direct Sunlight", value: "directsunlight" },
      { text: "Overcast Sky", value: "overcastsky" },
      { text: "Clear Blue Sky", value: "clearbluesky" },
      { text: "Warm Fluorescent", value: "warmfluorescent" },
      { text: "Standard Fluorescent", value: "standardfluorescent" },
      { text: "Cool White Fluorescent", value: "coolwhitefluorescent" },
      { text: "Full Spectrum Fluorescent", value: "fullspectrumfluorescent" },
      { text: "Grow Light Fluorescent", value: "growlightfluorescent" },
      { text: "Black Light Fluorescent", value: "blacklightfluorescent" },
      { text: "Mercury Vapor", value: "mercuryvapor" },
      { text: "Sodium Vapor", value: "sodiumvapor" },
      { text: "Metal Halide", value: "metalhalide" },
      { text: "High Pressure Sodium", value: "highpressuresodium" },
    ],
    ldrDevices: [
      { text: "GL5516", value: "GL5516" },
      { text: "GL5528", value: "GL5528" },
      { text: "GL5537_1", value: "GL5537_1" },
      { text: "GL5537_2", value: "GL5537_2" },
      { text: "GL5539", value: "GL5539" },
      { text: "GL5549", value: "GL5549" },
    ],
    pinsESP8266: [
      { text: "Pin D0", value: "Pin_D0" },
      { text: "Pin D1", value: "Pin_D1" },
      { text: "Pin D3", value: "Pin_D3" },
      { text: "Pin D4", value: "Pin_D4" },
      { text: "Pin D5", value: "Pin_D5" },
      { text: "Pin D6", value: "Pin_D6" },
      { text: "Pin D7", value: "Pin_D7" },
      { text: "Pin D8", value: "Pin_D8" },
    ],
  },
  getters: {
  },
  mutations: {
  },
  actions: {
  },
  modules: {
  }
})

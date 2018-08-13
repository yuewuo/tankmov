// The Vue build version to load with the `import` command
// (runtime-only or standalone) has been set in webpack.base.conf with an alias.
import Vue from 'vue'
import App from './App'
import ElementUI from 'element-ui'
import 'element-ui/lib/theme-chalk/index.css'
import VueResource from 'vue-resource'

Vue.config.productionTip = false
Vue.use(ElementUI)
Vue.use(VueResource)

/* eslint-disable no-new */
new Vue({
  el: '#app',
  components: { App },
  template: '<App/>',
  data: {
    setspeed: function () {
      
    }
  },
  methods: {
    setspeed: function (event) {
      if (event) {
        alert(event.target.tagName)
      }
    }
  }
})

let actions = {
  setSpeed: {method: 'POST', url: '/api/speed/'}
}
var esp32 = Vue.resource('', {}, actions)

esp32.setSpeed({speed: 100}).then(response => {
  // success callback
}, response => {
  // error callback
})

<template>
  <div id="app">
    <el-container>
      <el-header><img src="./assets/tank.png"><font>tankmov controller</font></el-header>
      <el-main>
        <el-button v-on:click="{{ setspeed }}">set speed</el-button>
      </el-main>
    </el-container>
  </div>
</template>

<script>
export default {
  name: 'App'
}
</script>

<style>
#app {
  font-family: 'Avenir', Helvetica, Arial, sans-serif;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
  color: #2c3e50;
  margin-top: 10px;
}

.el-header {
  font-family: 'Ubuntu', sans-serif;
  background-color: #B3C0D1;
  color: #333;
  line-height: 60px;
}

.el-header img {
  width: 90px;
  margin-bottom: -20px;
  margin-left: -15px;
}

.el-header font {
  margin-left: 20px;
  font-size: 22px;
}

.el-main {
  background-color: #E9EEF3;
  color: #333;
  text-align: center;
  height: 400px;
}

</style>

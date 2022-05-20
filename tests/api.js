import Vue from "vue";

const provider = {
  // whoami: () => {
  //   const config = Vue.prototype.$config
  //   const http = Vue.prototype.$http
  //   return http
  //     .get(config.api.url + `/user/me`)
  //     .then(res => {
  //       return Promise.resolve(res)
  //     })
  //     .catch(err => {
  //       console.log(err)
  //     })
  // },

  model: model => {
    return {
      load: id => {
        const config = Vue.prototype.$config;
        const http = Vue.prototype.$http;
        return http
          .get(config.api.url + `/${model}/${id}`)
          .then(res => {
            console.log(res);
            return Promise.resolve(res);
          })
          .catch(err => {
            console.log(err);
          });
      },

      save: doc => {
        const config = Vue.prototype.$config;
        const http = Vue.prototype.$http;
        let method = doc._id ? "put" : "post";
        let url = config.api.url + `/${model}/` + (doc._id ? doc._id : "");
        return http({
          method: method,
          url: url,
          data: doc
        })
          .then(res => {
            console.log(res.data);
            if (!res.data._id) {
              return Promise.reject(res);
            }
            return Promise.resolve(res);
          })
          .catch(err => {
            return Promise.reject(err);
          });
      },

      delete: id => {
        const config = Vue.prototype.$config;
        const http = Vue.prototype.$http;
        return http
          .delete(config.api.url + `/${model}/${id}`)
          .then(res => {
            return Promise.resolve(res);
          })
          .catch(err => {
            console.log(err);
          });
      },

      find: (params = {}) => {
        const config = Vue.prototype.$config;
        const http = Vue.prototype.$http;
        return http.get(config.api.url + `/${model}`, params);
      }
    };
  }
};

export default provider;

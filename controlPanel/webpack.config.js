var webpack = require('webpack');
var path = require('path');
var CompressionPlugin = require("compression-webpack-plugin");

var BUILD_DIR = path.resolve(__dirname, 'build');
var SRC_DIR = path.resolve(__dirname, 'source');

module.exports = {
	entry: SRC_DIR + '/index.js',
	output: {
		path: BUILD_DIR,
		filename: 'controlPanelApp.js'
	},
	module : {
		loaders : [{
			test: /\.js$/,
			exclude: /node_modules/,
			loader: 'babel-loader'
		}]
	},

	plugins: [
		new CompressionPlugin({
			asset: "[path].gz[query]",
			algorithm: "gzip",
			test: /\.js$|\.css$|\.html$/,
			threshold: 10240,
			minRatio: 0
		})
	],

	devServer: {
		publicPath: '/build'
	}
};

import React, { Component } from 'react';
import ReactDOM from 'react-dom';
import { PageHeader } from 'react-bootstrap';

export default class Lighting extends Component {

	constructor() {
		super();
		this.state = { lh1: [] };
	}

	render() {
		return (
		 	<div>
				<PageHeader>Освещение</PageHeader>
			</div>
		);
	}

	componentDidMount() {
		this.loadData();
	}

	loadData() {
		fetch('http://192.168.1.210/Status', { mode: 'cors'  })
			.then(responce => responce.text())
			.then(json => alert(json))
			//.then(json => { this.setState({ lh1: json }); })
			.catch(err => alert(err));
	}
}

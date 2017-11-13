import React, { Component } from 'react';	// eslint-disable-line no-unused-vars
import ReactDOM from 'react-dom';		// eslint-disable-line no-unused-vars
import { PageHeader } from 'react-bootstrap';	// eslint-disable-line no-unused-vars
import { Button } from 'react-bootstrap';	// eslint-disable-line no-unused-vars

export default class Lighting extends Component {

	constructor() {
		super();
		this.state = { sw: [undefined, undefined, undefined] };
	}

	render() {
		return (
			<div>
				<PageHeader>Освещение</PageHeader>
				Коридор 1:

				<Button bsStyle={ this.getStatusAttr(0, 0).btnStyle }
					onClick={ () => this.toggleStatus(0, 0) }
					bsSize="large">
					{this.getStatusAttr(0, 0).action}
				</Button>
			</div>
		);
	}

	getStatusAttr(buttonId, lineNumber) {

		var sw = this.state.sw[buttonId];
		if (typeof sw == 'object')
		{
			if (sw.Lines[lineNumber].Status == 1)
				return {
					action: 'Выключить',
					btnStyle: 'default'
				};
			else
				return {
					action: 'Включить',
					btnStyle: 'warning'
				};
		}
		else {
			return {
				action: '...',
				btnStyle: 'default'
			};
		}
	}

	toggleStatus(buttonId, lineNumber) {

		var sw = this.state.sw[buttonId];
		if (typeof sw == 'object')
		{
			var newState = (sw.Lines[lineNumber].Status == 0) ? 1 : 0;
			fetch(
				`http://192.168.1.210/ChangeLine?line=${lineNumber}&state=${newState}`,
				{ mode: 'cors' }
			)
				.then(() => {
					var nsw = this.state.sw.slice();
					nsw[buttonId].Lines[lineNumber].Status = newState;
					this.setState({ sw: nsw });
				})
				.catch(err => alert(err));
		}
	}

	componentDidMount() {
		this.loadData();
	}

	loadData() {
		fetch('http://192.168.1.210/Status', { mode: 'cors'  })
			.then(responce => responce.json())
			.then(status => {
				var nsw = this.state.sw.slice();
				nsw[0] = status;
				this.setState( { sw: nsw } );
			})
			.catch(err => alert(err));
	}
}

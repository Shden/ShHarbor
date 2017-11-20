import React, { Component } from 'react';		// eslint-disable-line no-unused-vars
import ReactDOM from 'react-dom';			// eslint-disable-line no-unused-vars
import { PageHeader } from 'react-bootstrap';		// eslint-disable-line no-unused-vars
import { Button } from 'react-bootstrap';		// eslint-disable-line no-unused-vars
import { Grid, Row, Col } from 'react-bootstrap';	// eslint-disable-line no-unused-vars

class LightingSwitch extends Component {		// eslint-disable-line no-unused-vars

	constructor() {
		super();
		this.state = { };
	}

	render() {
		return (
			<Grid>
				<Row>
					<Col xs={2}>
						<b>{this.props.name}:</b>
					</Col>
					<Col xs={6}>
						<Button bsStyle={ this.getStatusAttr().btnStyle }
							onClick={ () => this.toggleStatus() }
							bsSize="large">
							{ this.getStatusAttr().action }
						</Button>
					</Col>
				</Row>
			</Grid>
		);
	}

	getStatusAttr() {

		if (typeof this.state.Lines == 'object')
		{
			if (this.state.Lines[this.props.num].Status == 1)
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

	toggleStatus() {

		if (typeof this.state == 'object')
		{
			var newState = (this.state.Lines[this.props.num].Status == 0) ? 1 : 0;
			fetch(
				`http://${this.props.address}/ChangeLine?line=${this.props.num}&state=${newState}`,
				{ mode: 'cors' }
			)
				.then(() => {
					var ns = Object.assign({}, this.state);
					ns.Lines[this.props.num].Status = newState;
					this.setState({ ns });
				})
				.catch(err => alert(err));
		}
	}

	componentDidMount() {
		this.loadData();
	}

	loadData() {
		fetch(`http://${this.props.address}/Status`, { mode: 'cors'  })
			.then(responce => responce.json())
			.then(status => { this.setState(Object.assign({}, status)); })
			.catch(err => alert(err));
	}
}

export default class Lighting extends Component {

	render() {
		return (
			<div>
				<PageHeader>Освещение</PageHeader>
				<LightingSwitch name="Коридор 1" address="192.168.1.210" num="0" />
			</div>
		);
	}
}

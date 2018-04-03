import React, { Component } from 'react';		// eslint-disable-line no-unused-vars
import ReactDOM from 'react-dom';			// eslint-disable-line no-unused-vars
import { PageHeader } from 'react-bootstrap';		// eslint-disable-line no-unused-vars
import { Button } from 'react-bootstrap';		// eslint-disable-line no-unused-vars
import { Grid, Row, Col } from 'react-bootstrap';	// eslint-disable-line no-unused-vars

class RoomClimate extends Component { 			// eslint-disable-line no-unused-vars

	constructor() {
		super();
		this.state = { CurrentTemperature: '--.--', TargetTemperature: '--.--', Heating: '---' };
	}

	render() {
		return (
			<Grid>
				<Row>
					<Col xs={3}>
						<b>{this.props.name}:</b>
					</Col>
					<Col xs={8}>
						Температура {this.state.CurrentTemperature}&deg;C,
						настройка терморегулятора <a href={'http://' + this.props.address + '/config'}>{this.state.TargetTemperature}&deg;C</a>,
						нагрев {this.state.Heating ? 'включен' : 'выключен' }.
					</Col>
				</Row>
			</Grid>
		);
	}

	componentDidMount() {
		this.loadData();
	}

	loadData() {
		fetch(`http://${this.props.address}/Status`, { mode: 'cors'  })
			.then(responce => responce.json())
			.then(status => {
				this.setState(Object.assign({}, status ));
			})
			.catch(err => alert(err));
	}
}

export default class Heating extends Component {

	render() {
		return (
			<div>
				<PageHeader>Климат</PageHeader>
				<RoomClimate name="Cпальня" address="192.168.1.81"/>
				<RoomClimate name="Теплый пол в ванной" address="192.168.1.80"/>
			</div>
		);
	}
}

import React, { Component } from 'react';		// eslint-disable-line no-unused-vars
import ReactDOM from 'react-dom';			// eslint-disable-line no-unused-vars
import { PageHeader } from 'react-bootstrap';		// eslint-disable-line no-unused-vars
import { Button } from 'react-bootstrap';		// eslint-disable-line no-unused-vars
import { Grid, Row, Col } from 'react-bootstrap';	// eslint-disable-line no-unused-vars
import { Well } from 'react-bootstrap';			// eslint-disable-line no-unused-vars
import { Glyphicon } from 'react-bootstrap';		// eslint-disable-line no-unused-vars

class RoomClimate extends Component { 			// eslint-disable-line no-unused-vars

	constructor() {
		super();
		this.state = { CurrentTemperature: '--.--', TargetTemperature: '--.--', Heating: '---' };
	}

	render() {
		return (
			<Well bsSize="small">
				<Grid>
					<Row>
						<Col xs={4} md={3} lg={2}>
							<b>{this.props.name}:</b>
						</Col>
						<Col xs={8} md={7} lg={6}>
							Температура {this.state.CurrentTemperature}&deg;C,
							настройка терморегулятора <a href={'http://' + this.props.address + '/config'}>{this.state.TargetTemperature}&deg;C</a>,
							нагрев {this.state.Heating ? 'включен' : 'выключен' }
						</Col>
					</Row>
				</Grid>
			</Well>
		);
	}

	componentDidMount() {
		this.loadData();
	}

	loadData() {
		fetch(`http://${this.props.address}/status`)
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
				<PageHeader>
					Климат <Button bsStyle="primary" onClick={ () => this.reload() }>
						<Glyphicon glyph="glyphicon glyphicon-refresh"/> Обновить
					</Button>
				</PageHeader>
				<Grid>
					<Row>
						<Col xs={0} md={1} lg={2}/>
						<Col xs={12} md={10} lg={8}>
							<RoomClimate name="Теплый пол в ванной" address="192.168.1.80"/>
							<RoomClimate name="Cпальня" address="192.168.1.81"/>
							<RoomClimate name="Гостиная" address="192.168.1.82"/>
							<RoomClimate name="Комната Агаты" address="192.168.1.83"/>
							<RoomClimate name="Комната Cаши" address="192.168.1.84"/>
							<RoomClimate name="Кухня" address="192.168.1.85"/>
							<br/>

						</Col>
						<Col xs={0} md={1} lg={2}/>
					</Row>
				</Grid>
			</div>
		);
	}

	reload() {
		window.location.reload();
	}
}

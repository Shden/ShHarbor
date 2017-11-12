import React from 'react';					// eslint-disable-line no-unused-vars
import ReactDOM from 'react-dom';
import { HashRouter, Link, Route, Switch } from 'react-router-dom';

import { Navbar, Nav, NavItem } from 'react-bootstrap';		// eslint-disable-line no-unused-vars
import { PageHeader } from 'react-bootstrap';			// eslint-disable-line no-unused-vars
import { Grid, Row, Col } from 'react-bootstrap';		// eslint-disable-line no-unused-vars
import Lighting from './lighting.js';

const App = () => (
	<div>
		<Navbar inverse>
			<Navbar.Header>
				<Navbar.Brand>
					<a href="#">Гавань</a>
				</Navbar.Brand>
				<Navbar.Toggle />
			</Navbar.Header>
			<Navbar.Collapse>
				<Nav>
					<NavItem eventKey={1}><Link to="light">Освещение</Link></NavItem>
					<NavItem eventKey={2}><Link to="about">О системе</Link></NavItem>
				</Nav>
			</Navbar.Collapse>
		</Navbar>
		<Grid>
			<Row>
				<Col xs={12} md={12}>
					<Switch>
						<Route path='/light' component={Lighting}/>
						<Route path='/about' component={About}/>
					</Switch>
				</Col>
			</Row>
		</Grid>
	</div>
);

const About = () => (
	<div>
		<PageHeader>О системе</PageHeader>
		<p>Управление всем, что поддается управлению в доме.</p>
		<p>Версия: 0.1.</p>
		<p>Дата: 12-11-2017.</p>
		<p>Оды, хвалебные стихи, благодарственные псалмы и если что-то не так: <a href="mailto:denis.afanassiev@gmail.com">denis.afanassiev@gmail.com</a></p>
	</div>
);

ReactDOM.render((
	<HashRouter>
		<App />
	</HashRouter>
), document.getElementById('app'));
